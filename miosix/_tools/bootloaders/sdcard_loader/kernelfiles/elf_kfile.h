// TODO must edit: LICENSE

#pragma once

#include <stdexcept>
#include <vector>
#include <algorithm>

#include "kfile.h"
#include "../miosix/kernel/elf_types.h"
#include "string.h"

#ifdef WITH_FATFS

namespace miosix
{
    class ElfKernelFile : public KernelFile
    {
    public:
        ElfKernelFile(const std::string &dir, const std::string &fn, size_t size)
            : KernelFile(dir, fn, size) {}

        void load(void **relocationAddress, void **kernelFileStart, void **kernelFileEnd) override
        {
            *relocationAddress = nullptr;
            *kernelFileStart = nullptr;
            *kernelFileEnd = nullptr;

            // Read the ELF file into memory
            std::string fullPath = directory + "/" + filename;
            FILE *file = fopen(fullPath.c_str(), "rb");
            if (file == nullptr)
            {
                throw std::runtime_error("Failed to open ELF file: " + fullPath);
            }

            // Read the entire file into a buffer
            std::vector<uint8_t> elfData(filesize);
            size_t bytesRead = fread(elfData.data(), 1, filesize, file);
            fclose(file);

            if (bytesRead != filesize)
            {
                throw std::runtime_error("Failed to read complete ELF file");
            }

            // Parse ELF header
            if (filesize < sizeof(Elf32_Ehdr))
            {
                throw std::runtime_error("File too small to be a valid ELF");
            }

            const Elf32_Ehdr *header = reinterpret_cast<const Elf32_Ehdr *>(elfData.data());

            // Validate ELF magic number
            if (header->e_ident[0] != 0x7f || header->e_ident[1] != 'E' ||
                header->e_ident[2] != 'L' || header->e_ident[3] != 'F')
            {
                throw std::runtime_error("Invalid ELF magic number");
            }

            // Check for 32-bit ARM ELF
            if (header->e_ident[4] != 1 || header->e_machine != EM_ARM)
            {
                throw std::runtime_error("Not a 32-bit ARM ELF file");
            }

            // Parse ELF and extract both relocation address and packed binary
            Elf32_Addr relocationAddr;
            std::vector<uint8_t> packedBinary = parseElfAndPack(elfData, header, relocationAddr);

            // Allocate memory for the packed binary
            void *dest = malloc(packedBinary.size());
            if (dest == nullptr)
            {
                throw std::runtime_error("Failed to allocate memory for packed binary");
            }

            // Copy packed binary to allocated memory
            memcpy(dest, packedBinary.data(), packedBinary.size());

            // Set output parameters
            *kernelFileStart = dest;
            *kernelFileEnd = (void *)((uintptr_t)dest + packedBinary.size());
            *relocationAddress = (void *)relocationAddr;
        }

    private:
        std::vector<uint8_t> parseElfAndPack(const std::vector<uint8_t> &elfData, const Elf32_Ehdr *header, Elf32_Addr &relocationAddr)
        {
            // Initialize relocation address to entry point as fallback
            relocationAddr = header->e_entry;

            // Get program headers for loadable segments
            if (header->e_phoff + header->e_phnum * sizeof(Elf32_Phdr) > elfData.size())
            {
                throw std::runtime_error("Invalid program header table");
            }

            const Elf32_Phdr *programHeaders = reinterpret_cast<const Elf32_Phdr *>(
                elfData.data() + header->e_phoff);

            // Find all loadable segments
            struct Segment
            {
                Elf32_Addr vaddr;
                Elf32_Word memsz;
                Elf32_Word filesz;
                Elf32_Off offset;
            };

            std::vector<Segment> loadableSegments;
            
            for (int i = 0; i < header->e_phnum; ++i)
            {
                const Elf32_Phdr &phdr = programHeaders[i];
                if (phdr.p_type == PT_LOAD)
                {
                    loadableSegments.push_back({phdr.p_vaddr,
                                                phdr.p_memsz,
                                                phdr.p_filesz,
                                                phdr.p_offset});
                }
            }

            if (loadableSegments.empty())
            {
                throw std::runtime_error("No loadable segments found in ELF file");
            }

            // Try to find relocation address from .relocation_placement section
            if (header->e_shoff != 0 && header->e_shnum > 0 && 
                header->e_shoff + header->e_shnum * sizeof(Elf32_Shdr) <= elfData.size())
            {
                const Elf32_Shdr *sectionHeaders = reinterpret_cast<const Elf32_Shdr *>(
                    elfData.data() + header->e_shoff);

                // Check if we have a valid string table
                if (header->e_shstrndx < header->e_shnum)
                {
                    const Elf32_Shdr &stringTableHeader = sectionHeaders[header->e_shstrndx];
                    if (stringTableHeader.sh_offset + stringTableHeader.sh_size <= elfData.size())
                    {
                        const char *stringTable = reinterpret_cast<const char *>(
                            elfData.data() + stringTableHeader.sh_offset);

                        // Search for .relocation_placement section
                        for (int i = 0; i < header->e_shnum; ++i)
                        {
                            const Elf32_Shdr &section = sectionHeaders[i];

                            if (section.sh_name < stringTableHeader.sh_size)
                            {
                                const char *sectionName = stringTable + section.sh_name;

                                if (strcmp(sectionName, ".relocation_placement") == 0)
                                {
                                    // Found the relocation placement section
                                    if (section.sh_size >= sizeof(Elf32_Word) &&
                                        section.sh_offset + section.sh_size <= elfData.size())
                                    {
                                        // Read the relocation address (first 4 bytes of the section)
                                        const Elf32_Word *relocAddr = reinterpret_cast<const Elf32_Word *>(
                                            elfData.data() + section.sh_offset);
                                        relocationAddr = *relocAddr;
                                        break; // Found it, stop searching
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Pack loadable segments into binary format
            // Sort segments by virtual address
            std::sort(loadableSegments.begin(), loadableSegments.end(),
                      [](const Segment &a, const Segment &b)
                      { return a.vaddr < b.vaddr; });

            // Calculate total memory size needed
            Elf32_Addr baseAddr = loadableSegments[0].vaddr;
            Elf32_Addr endAddr = 0;

            for (const auto &seg : loadableSegments)
            {
                Elf32_Addr segEnd = seg.vaddr + seg.memsz;
                if (segEnd > endAddr)
                {
                    endAddr = segEnd;
                }
            }

            size_t totalSize = endAddr - baseAddr;
            std::vector<uint8_t> packedBinary(totalSize, 0); // Initialize with zeros (for BSS sections)

            // Copy segment data
            for (const auto &seg : loadableSegments)
            {
                if (seg.filesz > 0)
                {
                    if (seg.offset + seg.filesz > elfData.size())
                    {
                        throw std::runtime_error("Invalid segment file offset/size");
                    }

                    size_t destOffset = seg.vaddr - baseAddr;
                    if (destOffset + seg.filesz > packedBinary.size())
                    {
                        throw std::runtime_error("Segment extends beyond calculated size");
                    }

                    memcpy(packedBinary.data() + destOffset,
                           elfData.data() + seg.offset,
                           seg.filesz);
                }
            }

            return packedBinary;
        }
    };
}

#endif // WITH_FATFS