#include <iostream>
#include <exception>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <yaml-cpp/yaml.h>
#include <sndfile.h>

#include "structs.hpp"
#include "util.hpp"

/*
 * Converts a `buffer_addr` (word-address) to a byte-address.
 */
uint32_t word_addr_to_byte_offset(const sy::buffer_addr& addr) {
  return 2U * nbytes_to_u<uint32_t>(addr.addr);
}

static struct loop_mode {
  unsigned value;
  std::string name;
  bool loop;
} const loop_modes[] = {
  {0U, "unknown",      false}, /* fallback */
  {1U, "play-forward", false},
  {2U, "loop-forward", true}
};

const loop_mode& find_loop_mode(unsigned value) {
  auto iter = std::find_if(std::begin(loop_modes), std::end(loop_modes), [&](const loop_mode& m) {
    return m.value == value;
  });
  if (iter != std::end(loop_modes)) {
    return *iter;
  }
  return loop_modes[0];
}

std::string make_note_string(unsigned key) {
  static const char* keys[] = { "F#", "G", "G#", "A", "A#", "B", "C", "C#", "D", "D#", "E", "F" };

  int key_val = (int)key - 146;
  if (key_val < 0) {
    key_val += 146;
  }

  unsigned octave = (key_val - 6) / 12;

  return std::string(keys[key_val % 12]) + std::to_string(octave);
}

struct sample_item {
  unsigned volume;
  unsigned loop_mode;
  unsigned key;
  int pitch;
  unsigned sample_no;
  std::vector<int16_t> sample_data;

  void from_header(const sy::sample_header* h) {
    volume = 0x7F - h->volume;
    loop_mode = h->loop_mode;
    key = h->orig_key;
    pitch = h->pitch;

    // TODO: Read and write all the missing attributes
  }

  void to_header(sy::sample_header* h) const {
    h->volume = std::max<int>(0, std::abs((int)volume - 0x7F));
    h->loop_mode = loop_mode;
    h->orig_key = key;
    h->pitch = pitch;
  }

  void to_yaml(YAML::Emitter& out) const {
    out << YAML::BeginMap;
    out << YAML::Key << "volume"    << YAML::Value << volume;
    out << YAML::Key << "orig-key"  << YAML::Value << make_note_string(key);
    out << YAML::Key << "loop-mode" << YAML::Value << find_loop_mode(loop_mode).name;
    out << YAML::Key << "pitch"     << YAML::Value << pitch;
    out << YAML::EndMap;
  }
};

struct wave_item {
  std::string name;
  std::vector<sample_item> samples;

  void from_header(const sy::wave_header* h) {
    name = get_lstr(h->name);
  }

  void to_header(sy::wave_header* h) const {
    set_lstr(name, h->name);
  }

  void to_yaml(YAML::Emitter& out) const {
    out << YAML::BeginMap;
    out << YAML::Key << "name" << YAML::Value << name;
    out << YAML::Key << "sample" << YAML::Value;
    out << YAML::BeginSeq;
    std::for_each(samples.begin(), samples.end(), [&](const auto& sample) { sample.to_yaml(out); });
    out << YAML::EndSeq;
    out << YAML::EndMap;
  }
};

struct card_item {
  std::string name;
  unsigned id;
  std::vector<wave_item> waves;

  void from_header(const sy::file_header* h) {
    name = get_lstr(h->name);
    id = h->card_id;
  }

  void to_header(sy::file_header* h) const {
    set_lstr(name, h->name);
    h->card_id = id;
  }

  void to_yaml(YAML::Emitter& out) const {
    out << YAML::BeginMap;
    out << YAML::Key << "name" << YAML::Value << name;
    out << YAML::Key << "id" << YAML::Value << id;
    out << YAML::Key << "wave" << YAML::Value;
    out << YAML::BeginSeq;
    std::for_each(waves.begin(), waves.end(), [&](const auto& wave) { wave.to_yaml(out); });
    out << YAML::EndSeq;
    out << YAML::EndMap;
  }
};

std::vector<word_t> extract_sample_data(const std::vector<ubyte_t>& buffer, const sy::sample_header* h) {
  auto sample_begin_loc = word_addr_to_byte_offset(h->sample_begin);
  auto sample_end_loc = word_addr_to_byte_offset(h->sample_end);
  if (sample_begin_loc < buffer.size() &&
      sample_end_loc < buffer.size()) {
    auto sample_begin = reinterpret_cast<const word_t*>(buffer.data() + sample_begin_loc);
    auto sample_end = reinterpret_cast<const word_t*>(buffer.data() + sample_end_loc);
    if (sample_begin < sample_end) {
      std::vector<word_t> sample_data(sample_begin, sample_end);
      swap_word_endianess(sample_data); // Convert endianess from BE to LE.
      return sample_data;
    } else {
      throw std::runtime_error("Invalid sample buffer location.");
    }
  } else {
    throw std::runtime_error("Sample buffer location out of bounds.");
  }

  return {};
}

card_item load_buffer(const std::vector<ubyte_t>& buffer) {
  std::size_t loc = 0;
  auto file_h = buffer_struct_next<sy::file_header>(buffer, loc);

  std::vector<wave_item> waves;
  for (std::size_t i = 0; i < file_h->num_waves; ++i) {
    auto wave_a = buffer_struct_next<sy::wave_addr>(buffer, loc);

    std::size_t wave_loc = word_addr_to_byte_offset(wave_a->addr);
    auto wave_h = buffer_struct_next<sy::wave_header>(buffer, wave_loc);

    std::vector<sample_item> samples;
    for (std::size_t j = 0; j < wave_h->num_samples; ++j) {
      auto sample_h = buffer_struct_next<sy::sample_header>(buffer, wave_loc);

      sample_item si;
      si.from_header(sample_h);
      si.sample_data = extract_sample_data(buffer, sample_h);
      samples.push_back(std::move(si));
    }

    wave_item wi;
    wi.from_header(wave_h);
    wi.samples = std::move(samples);
    waves.push_back(std::move(wi));
  }

  card_item ci;
  ci.from_header(file_h);
  ci.waves = std::move(waves);
  return ci;
}

std::vector<ubyte_t> load_wave_card(const char* path) {
  std::ifstream f(path, std::ifstream::binary);

  std::vector<ubyte_t> raw(512);
  for (std::size_t off = 0; f.read(reinterpret_cast<char*>(raw.data()+off), 512); off+=512) {
    if (!f.eof())
      raw.resize(raw.size()+512);
  }

  swap_word_endianess(raw);
  return raw;
}

void write_sample(const std::string& path, const int16_t* buffer, std::size_t length) {
  SF_INFO info = {};
  info.samplerate = 32000; /* TODO: Is the samplerate fixed? */
  info.channels = 1;
  info.format = SF_FORMAT_WAV|SF_FORMAT_PCM_16|SF_ENDIAN_LITTLE;

  SNDFILE* sf = sf_open(path.c_str(), SFM_WRITE, &info);
  if (!sf) {
    throw std::runtime_error(sf_strerror(sf));
  }

  sf_write_short(sf, buffer, length);
  sf_write_sync(sf);
  sf_close(sf);
}

void unpack_samples(const card_item& card, std::string target_dir) {
  struct stat st = {0};
  if (stat(target_dir.c_str(), &st) == -1) {
    mkdir(target_dir.c_str(), 0700);
  }

  std::for_each(card.waves.begin(), card.waves.end(), [&, i = 0](const wave_item& wi) mutable {
    auto sample_dir = target_dir + "/";
    if (wi.name.empty()) {
      sample_dir += "unnamed_wave_" + std::to_string(i++);
      std::cerr << "Extracting samples of unnamed wave " << i << " to " << sample_dir << ".\n";
    } else {
      sample_dir += wi.name;
    }

    st = {0};
    if (stat(sample_dir.c_str(), &st) == -1) {
        mkdir(sample_dir.c_str(), 0700);
    }

    std::for_each(wi.samples.begin(), wi.samples.end(), [&, i = 0](const sample_item& si) mutable {
      auto sample_path = sample_dir + "/" + std::to_string(i++) + ".wav";

      write_sample(sample_path, si.sample_data.data(), si.sample_data.size());
    });
  });
}

void unpack_card(const card_item& card, std::string target_dir) {
  struct stat st = {0};
  if (stat(target_dir.c_str(), &st) != -1) {
    target_dir += "/" + card.name + ".wavecard";
  }
  mkdir(target_dir.c_str(), 0700);

  YAML::Emitter out;
  card.to_yaml(out);

  std::ofstream of(target_dir + "/data.yaml");
  of << out.c_str() << std::endl;

  unpack_samples(card, target_dir + "/samples");
}

void print_help() {
  std::cout <<
    "sycard COMMAND [ARGS...]\n\n" <<
    "COMMAND:\n" <<
    "  unpack CARD-FILE [TARGET]      Unpacks the CARD-FILE data into the TARGET directory.\n"
    "  pack SOURCE-DIR [CARD-FILE]    Packs the data of SOURCE-DIR into a new card-file at CARD-FILE.\n\n";
}

void die_usage(const char* msg) {
  std::cerr << msg << "\n\n";
  print_help();
  std::exit(-1);
}

int main(int argc, const char** argv)
{
  const char** arg = ++argv;
  if (*arg) {
    if (strcmp(*arg, "unpack") == 0) {
      if (!*++arg)
        die_usage("Missing argument CARD-FILE.");

      try {
        auto card = load_buffer(load_wave_card(*arg));
        auto target_path = *++arg ? *arg : ".";

        unpack_card(card, target_path);
      } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return -1;
      }
    } else if (strcmp(*arg, "pack") == 0) {
      // TODO: Implement
      die_usage("Feature not implemented.");
    } else {
      die_usage("Unknown command.");
    }
  }
  
  return 0;
}
