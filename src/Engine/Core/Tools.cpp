module Engine.Core.Tools;

import std;

import Engine.Core.Logger;

namespace Vyroda
{
  std::vector<char> Tools::readFile(const std::string & filename)
  {
    if (filename.empty() || !std::filesystem::is_regular_file(filename)) {
      Logger::warn("failed to open file (not a regular file): {}", filename);
      return {};
    }

    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
      Logger::warn("failed to open file: {}", filename);
      return {};
    }

    std::streamsize file_size = file.tellg();
    if (file_size <= 0) {
      return {};
    }

    std::vector<char> buffer(static_cast<unsigned long>(file_size));

    file.seekg(0);
    file.read(buffer.data(), file_size);

    file.close();

    return buffer;
  }
}
