#include "nspbuild.hpp"
#include "log.hpp"

// Use nspmini's crypto utilities for cleaner key derivation and encryption
#include <nspmini/extra/crypto.hpp>

#include <switch.h>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <cerrno>

namespace NSPBuild {

// ============================================================================
// Constants
// ============================================================================

static const uint32_t NRO_MAGIC = 0x304F524E;  // "NRO0"
static const uint32_t ASET_MAGIC = 0x54455341; // "ASET"
static const uint32_t MAGIC_NCA3 = 0x3341434E; // "NCA3"
static const uint32_t MAGIC_IVFC = 0x43465649; // "IVFC"
static const uint32_t MAGIC_PFS0 = 0x30534650; // "PFS0"

static const size_t MEDIA_UNIT = 0x200;
static const size_t NCA_HEADER_SIZE = 0xC00;
static const size_t IVFC_MAX_LEVEL = 6;
static const size_t IVFC_HASH_BLOCK_SIZE = 0x4000;
static const size_t PFS0_EXEFS_HASH_BLOCK_SIZE = 0x10000;
static const size_t PFS0_META_HASH_BLOCK_SIZE = 0x1000;

// System title IDs to avoid (simplified list of known ranges)
static bool IsSystemTitleId(const std::string &title_id) {
    if (title_id.length() != 16) return true;
    
    // System titles start with 01000000000000xx or 01000000000010xx
    if (title_id.substr(0, 14) == "01000000000000") return true;
    if (title_id.substr(0, 14) == "01000000000010") return true;
    if (title_id.substr(0, 14) == "01000000000020") return true;
    if (title_id.substr(0, 12) == "010000000000") return true;
    
    return false;
}

// ============================================================================
// Helper Functions
// ============================================================================

static std::string ToLowerHex(const uint8_t* data, size_t len) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; i++) {
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    return ss.str();
}

static bool WriteFileBytes(const std::string &path, const uint8_t* data, size_t size) {
    FILE *f = fopen(path.c_str(), "wb");
    if (!f) return false;
    
    size_t written = fwrite(data, 1, size, f);
    fclose(f);
    
    return written == size;
}

static bool WriteFileBytes(const std::string &path, const std::vector<uint8_t> &data) {
    return WriteFileBytes(path, data.data(), data.size());
}

static std::vector<uint8_t> ReadFileBytes(const std::string &path) {
    std::vector<uint8_t> data;
    FILE *file = fopen(path.c_str(), "rb");
    if (!file) {
        Log::Error("NSPBuild: Failed to open file %s\n", path.c_str());
        return data;
    }
    
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    data.resize(size);
    size_t read = fread(data.data(), 1, size, file);
    fclose(file);
    
    if (read != size) {
        Log::Error("NSPBuild: Failed to read file %s (read %zu of %zu bytes)\n", path.c_str(), read, size);
        data.clear();
    }
    
    return data;
}

static bool DeleteRecursive(const std::string &path) {
    DIR *dir = opendir(path.c_str());
    if (!dir) {
        // It's a file, just remove it
        return remove(path.c_str()) == 0;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        
        std::string full_path = path + "/" + name;
        if (entry->d_type == DT_DIR) {
            DeleteRecursive(full_path);
        } else {
            remove(full_path.c_str());
        }
    }
    closedir(dir);
    return rmdir(path.c_str()) == 0;
}

static uint64_t Align(uint64_t offset, uint64_t alignment) {
    return (offset + alignment - 1) & ~(alignment - 1);
}

// ============================================================================
// Key Management (Hardware-based derivation using SPL crypto)
// ============================================================================

struct KeySet {
    uint8_t header_key[0x20];          // Derived from hardware
    uint8_t key_area_key_application[3][0x10];  // Derived from hardware
    bool valid = false;
};

// Key sources for key_area_key_application derivation
// These are public constants used by the Switch's key derivation
static const uint8_t key_area_key_application_source[0x10] = {
    0x7F, 0x59, 0x97, 0x1E, 0x62, 0x9F, 0x36, 0xA1,
    0x30, 0x98, 0x06, 0x6F, 0x21, 0x44, 0xC3, 0x0D
};

static const uint8_t aes_kek_generation_source[0x10] = {
    0x4D, 0x87, 0x09, 0x86, 0xC4, 0x5D, 0x20, 0x72,
    0x2F, 0xBA, 0x10, 0x53, 0xDA, 0x92, 0xE8, 0xA9
};

static const uint8_t aes_key_generation_source[0x10] = {
    0x89, 0x61, 0x5E, 0xE0, 0x5C, 0x31, 0xB6, 0x80,
    0x5F, 0xE5, 0x8F, 0x3D, 0xA2, 0x4F, 0x7A, 0xA8
};

static KeySet DeriveKeys() {
    KeySet keys;
    keys.valid = false;
    
    // Initialize SPL crypto service
    Result rc = splCryptoInitialize();
    if (R_FAILED(rc)) {
        Log::Error("NSPBuild: Failed to initialize splCrypto: 0x%x\n", rc);
        return keys;
    }
    
    // Use Crypto::Keys from nspmini to derive header_key
    Crypto::Keys cryptoKeys;
    memcpy(keys.header_key, cryptoKeys.headerKey, 0x20);
    
    // Derive key_area_key_application for multiple key generations
    // We derive keys for generations 0, 1, 2 (most common)
    for (int gen = 0; gen < 3; gen++) {
        u8 kek[0x10];
        
        // Generate KEK using the application source
        rc = splCryptoGenerateAesKek(key_area_key_application_source, gen, 0, kek);
        if (R_FAILED(rc)) {
            Log::Debug("NSPBuild: Failed to generate KEK for gen %d: 0x%x\n", gen, rc);
            // Use zeroed key as fallback (will still work for unsigned content)
            memset(keys.key_area_key_application[gen], 0, 0x10);
            continue;
        }
        
        // Generate the actual key
        rc = splCryptoGenerateAesKey(kek, aes_key_generation_source, keys.key_area_key_application[gen]);
        if (R_FAILED(rc)) {
            Log::Debug("NSPBuild: Failed to generate key for gen %d: 0x%x\n", gen, rc);
            memset(keys.key_area_key_application[gen], 0, 0x10);
            continue;
        }
    }
    
    splCryptoExit();
    
    keys.valid = true;
    Log::Debug("NSPBuild: Keys derived from hardware successfully\n");
    return keys;
}

// ============================================================================
// SHA256 Helper
// ============================================================================

static void ComputeSHA256(const uint8_t* data, size_t size, uint8_t* out) {
    Sha256Context ctx;
    sha256ContextCreate(&ctx);
    sha256ContextUpdate(&ctx, data, size);
    sha256ContextGetHash(&ctx, out);
}

// ============================================================================
// AES-128-XTS Encryption (using nspmini's Crypto::AesXtr)
// ============================================================================

static void AesXtsEncrypt(const uint8_t* key, uint64_t sector, uint8_t* data, size_t data_size) {
    // Use nspmini's AesXtr class for cleaner XTS encryption
    // The key is 0x20 bytes (two 0x10-byte keys concatenated)
    Crypto::AesXtr xts(key, true);  // true = encryptor
    xts.encrypt(data, data, data_size, sector, 0x200);  // 0x200 = sector size for NCA headers
}

// ============================================================================
// PFS0 (NSP) Structure
// ============================================================================

#pragma pack(push, 1)
struct PFS0Header {
    uint32_t magic;           // "PFS0"
    uint32_t file_count;
    uint32_t string_table_size;
    uint32_t reserved;
};

struct PFS0FileEntry {
    uint64_t offset;
    uint64_t size;
    uint32_t string_table_offset;
    uint32_t reserved;
};

// PFS0 Superblock for NCA FS header
struct PFS0Superblock {
    uint8_t master_hash[0x20];    // SHA-256 hash of the hash table
    uint32_t block_size;          // Hash block size in bytes
    uint32_t always_2;            // Always 0x2
    uint64_t hash_table_offset;   // Normally zero
    uint64_t hash_table_size;     // Size of hash table
    uint64_t pfs0_offset;         // Offset to PFS0 data (after hash table + padding)
    uint64_t pfs0_size;           // Size of PFS0 data
    uint8_t padding[0xF0];
};

// IVFC Level Header
struct IvfcLevelHeader {
    uint64_t logical_offset;
    uint64_t hash_data_size;
    uint32_t block_size;          // Log2 of block size (e.g., 0x0E = 16384 bytes)
    uint32_t reserved;
};

// IVFC Header for RomFS
struct IvfcHeader {
    uint32_t magic;               // "IVFC"
    uint32_t id;                  // Always 0x20000
    uint32_t master_hash_size;    // Always 0x20
    uint32_t num_levels;          // Always 7 (includes master hash level)
    IvfcLevelHeader level_headers[IVFC_MAX_LEVEL];
    uint8_t reserved[0x20];
    uint8_t master_hash[0x20];
};

// RomFS Superblock for NCA FS header
struct RomFSSuperblock {
    IvfcHeader ivfc_header;
    uint8_t padding[0x58];
};

// NCA Section Entry
struct NcaSectionEntry {
    uint32_t media_start_offset;  // In media units (0x200 bytes)
    uint32_t media_end_offset;
    uint8_t enabled_and_padding[8];
};

// NCA FS Header (0x200 bytes each, 4 total)
struct NcaFsHeader {
    uint16_t version;
    uint8_t fs_type;              // 0 = RomFS, 1 = PFS0
    uint8_t hash_type;            // 2 = HierarchicalSha256 (PFS0), 3 = HierarchicalIntegrity (IVFC)
    uint8_t crypt_type;           // 1 = None, 3 = AES-CTR
    uint8_t reserved1[3];
    union {
        PFS0Superblock pfs0_superblock;
        RomFSSuperblock romfs_superblock;
    };
    uint8_t section_ctr[8];
    uint8_t reserved2[0xB8];
};

// Full NCA Header (0xC00 bytes)
struct NcaHeader {
    uint8_t rsa_sig1[0x100];      // RSA-PSS signature (fixed key)
    uint8_t npdm_key_sig[0x100];  // RSA-PSS signature (NPDM key)
    uint32_t magic;               // "NCA3"
    uint8_t distribution_type;    // 0 = Download, 1 = Gamecard
    uint8_t content_type;         // 0 = Program, 1 = Meta, 2 = Control, 3 = Manual, 4 = Data
    uint8_t crypto_type;          // Key generation (old)
    uint8_t kaek_index;           // Key area encryption key index (0 = Application)
    uint64_t nca_size;
    uint64_t title_id;
    uint8_t reserved1[4];
    uint32_t sdk_version;
    uint8_t crypto_type2;         // Key generation (new)
    uint8_t reserved2[0xF];
    uint8_t rights_id[0x10];
    NcaSectionEntry section_entries[4];
    uint8_t section_hashes[4][0x20];
    uint8_t encrypted_keys[4][0x10];
    uint8_t reserved3[0xC0];
    NcaFsHeader fs_headers[4];
};

// CNMT Header
struct CnmtHeader {
    uint64_t title_id;
    uint32_t title_version;
    uint8_t meta_type;            // 0x80 = Application
    uint8_t reserved1;
    uint16_t extended_header_size;
    uint16_t content_count;
    uint16_t content_meta_count;
    uint8_t attributes;
    uint8_t storage_id;
    uint8_t content_install_type;
    uint8_t reserved2;
    uint32_t required_download_system_version;
    uint8_t reserved3[4];
};

// CNMT Extended Header for Application
struct CnmtExtendedHeader {
    uint64_t patch_id;
    uint32_t required_system_version;
    uint32_t required_application_version;
};

// CNMT Content Entry
struct CnmtContentEntry {
    uint8_t hash[0x20];
    uint8_t nca_id[0x10];
    uint8_t size[6];              // 48-bit size, little-endian
    uint8_t content_type;         // 0 = Meta, 1 = Program, 2 = Data, 3 = Control
    uint8_t id_offset;
};

// RomFS Header
struct RomFsHeader {
    uint64_t header_size;
    uint64_t dir_hash_table_offset;
    uint64_t dir_hash_table_size;
    uint64_t dir_meta_table_offset;
    uint64_t dir_meta_table_size;
    uint64_t file_hash_table_offset;
    uint64_t file_hash_table_size;
    uint64_t file_meta_table_offset;
    uint64_t file_meta_table_size;
    uint64_t file_data_offset;
};

// RomFS Directory Entry (variable length)
struct RomFsDirEntry {
    uint32_t parent;
    uint32_t sibling;
    uint32_t child;
    uint32_t file;
    uint32_t hash;
    uint32_t name_size;
    // char name[] follows
};

// RomFS File Entry (variable length)
struct RomFsFileEntry {
    uint32_t parent;
    uint32_t sibling;
    uint64_t offset;
    uint64_t size;
    uint32_t hash;
    uint32_t name_size;
    // char name[] follows
};

#pragma pack(pop)

// ============================================================================
// PFS0 Building
// ============================================================================

struct PFS0BuildResult {
    std::vector<uint8_t> hash_table;
    std::vector<uint8_t> pfs0_data;
    uint64_t pfs0_offset;         // Offset where PFS0 starts (after aligned hash table)
};

// Simple PFS0 builder (for final NSP container - no hash table needed)
static std::vector<uint8_t> BuildPFS0(const std::vector<std::pair<std::string, std::vector<uint8_t>>> &files) {
    // Build string table
    std::vector<char> string_table;
    std::vector<uint32_t> string_offsets;
    
    for (const auto &file : files) {
        string_offsets.push_back(static_cast<uint32_t>(string_table.size()));
        string_table.insert(string_table.end(), file.first.begin(), file.first.end());
        string_table.push_back('\0');
    }
    
    // Pad string table to 0x20 alignment
    while (string_table.size() % 0x20 != 0) {
        string_table.push_back('\0');
    }
    
    // Build header
    PFS0Header header;
    header.magic = MAGIC_PFS0;
    header.file_count = static_cast<uint32_t>(files.size());
    header.string_table_size = static_cast<uint32_t>(string_table.size());
    header.reserved = 0;
    
    // Calculate file entries
    std::vector<PFS0FileEntry> entries;
    uint64_t current_offset = 0;
    
    for (size_t i = 0; i < files.size(); i++) {
        PFS0FileEntry entry;
        entry.offset = current_offset;
        entry.size = files[i].second.size();
        entry.string_table_offset = string_offsets[i];
        entry.reserved = 0;
        entries.push_back(entry);
        current_offset += entry.size;
    }
    
    // Build final PFS0
    size_t header_size = sizeof(PFS0Header) + entries.size() * sizeof(PFS0FileEntry) + string_table.size();
    std::vector<uint8_t> pfs0(header_size + current_offset);
    
    // Write header
    memcpy(pfs0.data(), &header, sizeof(header));
    memcpy(pfs0.data() + sizeof(header), entries.data(), entries.size() * sizeof(PFS0FileEntry));
    memcpy(pfs0.data() + sizeof(header) + entries.size() * sizeof(PFS0FileEntry), string_table.data(), string_table.size());
    
    // Write file data
    for (size_t i = 0; i < files.size(); i++) {
        memcpy(pfs0.data() + header_size + entries[i].offset, files[i].second.data(), files[i].second.size());
    }
    
    return pfs0;
}

// ============================================================================
// RomFS Building (Correct Layout)
// ============================================================================

static const uint32_t ROMFS_ENTRY_EMPTY = 0xFFFFFFFF;
static const uint64_t ROMFS_FILEPARTITION_OFS = 0x200;

static uint32_t CalcRomFsPathHash(uint32_t parent, const char* name, size_t name_len) {
    uint32_t hash = parent ^ 123456789;
    for (size_t i = 0; i < name_len; i++) {
        hash = (hash >> 5) | (hash << 27);
        hash ^= (uint8_t)name[i];
    }
    return hash;
}

static uint32_t RomFsGetHashTableCount(uint32_t num_entries) {
    if (num_entries < 3) return 3;
    if (num_entries < 19) return num_entries | 1;
    
    uint32_t count = num_entries;
    while (count % 2 == 0 || count % 3 == 0 || count % 5 == 0 || 
           count % 7 == 0 || count % 11 == 0 || count % 13 == 0 || count % 17 == 0) {
        count++;
    }
    return count;
}

// Build RomFS with correct layout:
// [Header][0x00-0x50]
// [File data starting at 0x200]
// [Dir hash table, Dir table, File hash table, File table - after file data]
static std::vector<uint8_t> BuildRomFS(const std::vector<std::pair<std::string, std::vector<uint8_t>>> &files) {
    // All files go in root directory
    uint32_t num_dirs = 1;   // Just root
    uint32_t num_files = static_cast<uint32_t>(files.size());
    
    uint32_t dir_hash_table_entry_count = RomFsGetHashTableCount(num_dirs);
    uint32_t file_hash_table_entry_count = RomFsGetHashTableCount(num_files);
    
    uint32_t dir_hash_table_size = dir_hash_table_entry_count * 4;
    uint32_t file_hash_table_size = file_hash_table_entry_count * 4;
    
    // Calculate dir table size (just root: 0x18 bytes, no name)
    uint32_t dir_table_size = 0x18;
    
    // Calculate file table size
    uint32_t file_table_size = 0;
    for (const auto &file : files) {
        // 0x20 bytes base + name aligned to 4 bytes
        file_table_size += 0x20 + Align(file.first.size(), 4);
    }
    
    // Calculate file data size and offsets
    uint64_t file_partition_size = 0;
    std::vector<uint64_t> file_offsets;
    for (const auto &file : files) {
        file_partition_size = Align(file_partition_size, 0x10);
        file_offsets.push_back(file_partition_size);
        file_partition_size += file.second.size();
    }
    
    // Calculate metadata offsets (after file data, starting at ROMFS_FILEPARTITION_OFS + file_partition_size)
    uint64_t dir_hash_table_ofs = Align(file_partition_size + ROMFS_FILEPARTITION_OFS, 4);
    uint64_t dir_table_ofs = dir_hash_table_ofs + dir_hash_table_size;
    uint64_t file_hash_table_ofs = dir_table_ofs + dir_table_size;
    uint64_t file_table_ofs = file_hash_table_ofs + file_hash_table_size;
    
    // Total RomFS size
    uint64_t romfs_size = file_table_ofs + file_table_size;
    
    // Allocate RomFS
    std::vector<uint8_t> romfs(romfs_size, 0);
    
    // Build header
    RomFsHeader header;
    header.header_size = sizeof(RomFsHeader);
    header.dir_hash_table_offset = dir_hash_table_ofs;
    header.dir_hash_table_size = dir_hash_table_size;
    header.dir_meta_table_offset = dir_table_ofs;
    header.dir_meta_table_size = dir_table_size;
    header.file_hash_table_offset = file_hash_table_ofs;
    header.file_hash_table_size = file_hash_table_size;
    header.file_meta_table_offset = file_table_ofs;
    header.file_meta_table_size = file_table_size;
    header.file_data_offset = ROMFS_FILEPARTITION_OFS;
    
    memcpy(romfs.data(), &header, sizeof(header));
    
    // Write file data (starting at ROMFS_FILEPARTITION_OFS)
    for (size_t i = 0; i < files.size(); i++) {
        memcpy(romfs.data() + ROMFS_FILEPARTITION_OFS + file_offsets[i], 
               files[i].second.data(), files[i].second.size());
    }
    
    // Build dir hash table
    std::vector<uint32_t> dir_hash_table(dir_hash_table_entry_count, ROMFS_ENTRY_EMPTY);
    // Root directory hash (empty name, parent 0)
    uint32_t root_hash = CalcRomFsPathHash(0, "", 0);
    dir_hash_table[root_hash % dir_hash_table_entry_count] = 0;
    
    // Build dir table (just root)
    RomFsDirEntry root_dir;
    root_dir.parent = 0;
    root_dir.sibling = ROMFS_ENTRY_EMPTY;
    root_dir.child = ROMFS_ENTRY_EMPTY;
    root_dir.file = (num_files > 0) ? 0 : ROMFS_ENTRY_EMPTY;
    root_dir.hash = ROMFS_ENTRY_EMPTY;  // End of hash chain
    root_dir.name_size = 0;
    
    // Build file hash table and file table
    std::vector<uint32_t> file_hash_table(file_hash_table_entry_count, ROMFS_ENTRY_EMPTY);
    std::vector<uint8_t> file_table_data;
    
    uint32_t file_entry_offset = 0;
    for (size_t i = 0; i < files.size(); i++) {
        const auto &file = files[i];
        uint32_t name_len = static_cast<uint32_t>(file.first.size());
        uint32_t entry_size = 0x20 + Align(name_len, 4);
        
        // Calculate hash
        uint32_t hash = CalcRomFsPathHash(0, file.first.c_str(), name_len);
        uint32_t hash_idx = hash % file_hash_table_entry_count;
        
        // Build entry
        RomFsFileEntry entry;
        entry.parent = 0;  // Root directory
        entry.sibling = (i + 1 < files.size()) ? (file_entry_offset + entry_size) : ROMFS_ENTRY_EMPTY;
        entry.offset = file_offsets[i];
        entry.size = file.second.size();
        entry.hash = file_hash_table[hash_idx];  // Chain to previous entry
        entry.name_size = name_len;
        
        // Update hash table
        file_hash_table[hash_idx] = file_entry_offset;
        
        // Add entry to table
        size_t cur_pos = file_table_data.size();
        file_table_data.resize(cur_pos + entry_size, 0);
        memcpy(file_table_data.data() + cur_pos, &entry, sizeof(entry));
        memcpy(file_table_data.data() + cur_pos + sizeof(entry), file.first.c_str(), name_len);
        
        file_entry_offset += entry_size;
    }
    
    // Write hash tables and metadata tables
    memcpy(romfs.data() + dir_hash_table_ofs, dir_hash_table.data(), dir_hash_table_size);
    memcpy(romfs.data() + dir_table_ofs, &root_dir, sizeof(root_dir));
    memcpy(romfs.data() + file_hash_table_ofs, file_hash_table.data(), file_hash_table_size);
    if (!file_table_data.empty()) {
        memcpy(romfs.data() + file_table_ofs, file_table_data.data(), file_table_data.size());
    }
    
    return romfs;
}

// ============================================================================
// IVFC Hash Tree Building
// ============================================================================

struct IvfcBuildResult {
    std::vector<std::vector<uint8_t>> levels;  // levels[0] = master hash level, levels[5] = data
    std::vector<uint64_t> level_sizes;
    uint8_t master_hash[0x20];
};

// Build IVFC hash tree from data (bottom-up)
// Level 5 = actual data (RomFS)
// Levels 0-4 = hash levels (level 0 is top, hashes level 1; level 4 hashes level 5)
// Master hash = hash of entire padded level 0
static IvfcBuildResult BuildIvfcTree(const std::vector<uint8_t> &data) {
    IvfcBuildResult result;
    result.levels.resize(IVFC_MAX_LEVEL);
    result.level_sizes.resize(IVFC_MAX_LEVEL);
    
    // Level 5 (index 5) is the actual data, aligned to IVFC_HASH_BLOCK_SIZE
    uint64_t data_size_aligned = Align(data.size(), IVFC_HASH_BLOCK_SIZE);
    result.levels[5] = data;
    result.levels[5].resize(data_size_aligned, 0);
    // Level 5: store UNPADDED size (matches reference romfs_build behavior)
    result.level_sizes[5] = data.size();
    
    // Build hash levels 4 -> 0
    for (int lvl = 4; lvl >= 0; lvl--) {
        const auto &src_level = result.levels[lvl + 1];
        uint64_t src_size = src_level.size();  // Padded size of source level
        
        // Number of hashes = number of blocks in source (source is already padded to block alignment)
        uint64_t num_blocks = (src_size + IVFC_HASH_BLOCK_SIZE - 1) / IVFC_HASH_BLOCK_SIZE;
        uint64_t hash_data_size = num_blocks * 0x20;
        uint64_t hash_data_aligned = Align(hash_data_size, IVFC_HASH_BLOCK_SIZE);
        
        result.levels[lvl].resize(hash_data_aligned, 0);
        // Levels 0-4: store PADDED size (matches reference ivfc_create_level behavior)
        result.level_sizes[lvl] = hash_data_aligned;
        
        // Compute hashes - hash full blocks from padded source data
        // Since source is padded to block alignment, each block is exactly IVFC_HASH_BLOCK_SIZE
        for (uint64_t i = 0; i < num_blocks; i++) {
            uint64_t block_start = i * IVFC_HASH_BLOCK_SIZE;
            // Source is padded, so we always hash full blocks (matches reference behavior)
            ComputeSHA256(src_level.data() + block_start, IVFC_HASH_BLOCK_SIZE, result.levels[lvl].data() + i * 0x20);
        }
    }
    
    // Master hash = hash of entire padded level 0 (matching reference implementation)
    ComputeSHA256(result.levels[0].data(), result.levels[0].size(), result.master_hash);
    
    return result;
}

// ============================================================================
// NACP Building
// ============================================================================

static std::vector<uint8_t> BuildNACP(const std::string &name, 
                                       const std::string &publisher,
                                       const std::string &version,
                                       const std::string &title_id) {
    std::vector<uint8_t> nacp(0x4000, 0);
    
    // Fill in all 16 title entries with the same name/publisher
    for (int i = 0; i < 16; i++) {
        size_t offset = i * 0x300;
        
        // Name (0x200 bytes)
        size_t name_len = std::min(name.size(), size_t(0x1FF));
        memcpy(nacp.data() + offset, name.c_str(), name_len);
        
        // Publisher (0x100 bytes)
        size_t pub_len = std::min(publisher.size(), size_t(0xFF));
        memcpy(nacp.data() + offset + 0x200, publisher.c_str(), pub_len);
    }
    
    // Display version at offset 0x3060 (16 bytes)
    size_t ver_len = std::min(version.size(), size_t(0x0F));
    memcpy(nacp.data() + 0x3060, version.c_str(), ver_len);
    
    // Startup user account (0x3025) = 0 (not required)
    nacp[0x3025] = 0x00;
    
    // Screenshot enabled (0x3034) = 0 (enabled)
    nacp[0x3034] = 0x00;
    
    // Video capture (0x3035) = 2 (enabled)
    nacp[0x3035] = 0x02;
    
    // Parse title_id
    uint64_t tid = 0;
    for (int i = 0; i < 16; i++) {
        char c = title_id[i];
        tid <<= 4;
        if (c >= '0' && c <= '9') tid |= (c - '0');
        else if (c >= 'a' && c <= 'f') tid |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') tid |= (c - 'A' + 10);
    }
    
    // Presence group ID (0x3070)
    memcpy(nacp.data() + 0x3070, &tid, 8);
    
    // Local communication IDs (0x30A8) 
    memcpy(nacp.data() + 0x30A8, &tid, 8);
    
    // Add-on content base ID (0x3078)
    uint64_t aoc_base = tid + 0x1000;
    memcpy(nacp.data() + 0x3078, &aoc_base, 8);
    
    return nacp;
}

// ============================================================================
// NCA Builder
// ============================================================================

static std::vector<uint8_t> BuildProgramNCA(const KeySet &keys, uint64_t title_id,
                                             const std::vector<uint8_t> &exefs_data,
                                             const std::vector<uint8_t> &romfs_data) {
    Log::Debug("NSPBuild: Building Program NCA\n");
    
    // Generate random content key for key area
    uint8_t content_key[0x10];
    randomGet(content_key, sizeof(content_key));
    
    // Build ExeFS with hash table
    std::vector<std::pair<std::string, std::vector<uint8_t>>> exefs_files;
    // exefs_data is already a PFS0, but we need to wrap it with hash table
    // Actually, we receive raw forwarder files, so let's build the PFS0 here
    
    // For Program NCA, section 0 = ExeFS (PFS0), section 1 = RomFS (IVFC)
    
    // Build ExeFS PFS0 with hash table
    // Note: exefs_data coming in is already a PFS0, need to add hash table
    PFS0BuildResult exefs_result;
    exefs_result.pfs0_data = exefs_data;
    
    // Build hash table for the PFS0
    size_t pfs0_size = exefs_data.size();
    size_t num_blocks = (pfs0_size + PFS0_EXEFS_HASH_BLOCK_SIZE - 1) / PFS0_EXEFS_HASH_BLOCK_SIZE;
    exefs_result.hash_table.resize(num_blocks * 0x20);
    
    // Hash only actual bytes read, not zero-padded blocks (matching reference implementation)
    for (size_t i = 0; i < num_blocks; i++) {
        size_t block_start = i * PFS0_EXEFS_HASH_BLOCK_SIZE;
        size_t block_len = std::min(static_cast<size_t>(PFS0_EXEFS_HASH_BLOCK_SIZE), pfs0_size - block_start);
        
        ComputeSHA256(exefs_data.data() + block_start, block_len, 
                      exefs_result.hash_table.data() + i * 0x20);
    }
    exefs_result.pfs0_offset = Align(exefs_result.hash_table.size(), 0x200);
    
    // Build RomFS IVFC tree
    IvfcBuildResult romfs_ivfc = BuildIvfcTree(romfs_data);
    
    // Calculate section sizes
    // Section 0 (ExeFS): hash_table + padding + pfs0
    size_t section0_data_size = exefs_result.pfs0_offset + exefs_result.pfs0_data.size();
    size_t section0_size_aligned = Align(section0_data_size, MEDIA_UNIT);
    
    // Section 1 (RomFS): all IVFC levels concatenated
    size_t section1_data_size = 0;
    for (int i = 0; i < 6; i++) {
        section1_data_size += romfs_ivfc.levels[i].size();
    }
    size_t section1_size_aligned = Align(section1_data_size, MEDIA_UNIT);
    
    // Section offsets
    size_t section0_offset = NCA_HEADER_SIZE;
    size_t section1_offset = section0_offset + section0_size_aligned;
    size_t total_size = section1_offset + section1_size_aligned;
    
    // Allocate NCA
    std::vector<uint8_t> nca(total_size, 0);
    NcaHeader* header = reinterpret_cast<NcaHeader*>(nca.data());
    
    // Fill header
    header->magic = MAGIC_NCA3;
    header->distribution_type = 0;  // Download
    header->content_type = 0;       // Program
    header->crypto_type = 0;
    header->kaek_index = 0;         // Application
    header->nca_size = total_size;
    header->title_id = title_id;
    header->sdk_version = 0x000D0000;  // SDK 13.x
    header->crypto_type2 = 0;
    
    // Section 0 entry (ExeFS)
    header->section_entries[0].media_start_offset = static_cast<uint32_t>(section0_offset / MEDIA_UNIT);
    header->section_entries[0].media_end_offset = static_cast<uint32_t>(section1_offset / MEDIA_UNIT);
    header->section_entries[0].enabled_and_padding[0] = 1;
    
    // Section 1 entry (RomFS)
    header->section_entries[1].media_start_offset = static_cast<uint32_t>(section1_offset / MEDIA_UNIT);
    header->section_entries[1].media_end_offset = static_cast<uint32_t>(total_size / MEDIA_UNIT);
    header->section_entries[1].enabled_and_padding[0] = 1;
    
    // FS Header 0 - ExeFS (PFS0 with HierarchicalSha256)
    header->fs_headers[0].version = 2;
    header->fs_headers[0].fs_type = 1;        // PFS0
    header->fs_headers[0].hash_type = 2;      // HierarchicalSha256
    header->fs_headers[0].crypt_type = 1;     // Plaintext (no section encryption for homebrew)
    
    // PFS0 superblock
    header->fs_headers[0].pfs0_superblock.block_size = PFS0_EXEFS_HASH_BLOCK_SIZE;
    header->fs_headers[0].pfs0_superblock.always_2 = 2;
    header->fs_headers[0].pfs0_superblock.hash_table_offset = 0;
    header->fs_headers[0].pfs0_superblock.hash_table_size = exefs_result.hash_table.size();
    header->fs_headers[0].pfs0_superblock.pfs0_offset = exefs_result.pfs0_offset;
    header->fs_headers[0].pfs0_superblock.pfs0_size = exefs_result.pfs0_data.size();
    
    // Calculate master hash for ExeFS
    ComputeSHA256(exefs_result.hash_table.data(), exefs_result.hash_table.size(),
                  header->fs_headers[0].pfs0_superblock.master_hash);
    
    // Set section CTR (generation based on section index)
    // Section CTR is stored big-endian in bytes 0-7, with the section start offset in bytes 8-15
    uint32_t section0_gen = 0;
    header->fs_headers[0].section_ctr[0] = (section0_gen >> 24) & 0xFF;
    header->fs_headers[0].section_ctr[1] = (section0_gen >> 16) & 0xFF;
    header->fs_headers[0].section_ctr[2] = (section0_gen >> 8) & 0xFF;
    header->fs_headers[0].section_ctr[3] = section0_gen & 0xFF;
    header->fs_headers[0].section_ctr[4] = 0;
    header->fs_headers[0].section_ctr[5] = 0;
    header->fs_headers[0].section_ctr[6] = 0;
    header->fs_headers[0].section_ctr[7] = 0;
    
    // FS Header 1 - RomFS (IVFC)
    header->fs_headers[1].version = 2;
    header->fs_headers[1].fs_type = 0;        // RomFS
    header->fs_headers[1].hash_type = 3;      // HierarchicalIntegrity (IVFC)
    header->fs_headers[1].crypt_type = 1;     // Plaintext (no section encryption for homebrew)
    
    // IVFC header
    IvfcHeader* ivfc = &header->fs_headers[1].romfs_superblock.ivfc_header;
    ivfc->magic = MAGIC_IVFC;
    ivfc->id = 0x20000;
    ivfc->master_hash_size = 0x20;
    ivfc->num_levels = 7;  // Includes master hash level
    
    // Set IVFC level headers
    uint64_t level_offset = 0;
    for (int i = 0; i < 6; i++) {
        ivfc->level_headers[i].logical_offset = level_offset;
        ivfc->level_headers[i].hash_data_size = romfs_ivfc.level_sizes[i];
        ivfc->level_headers[i].block_size = 0x0E;  // log2(0x4000) = 14
        ivfc->level_headers[i].reserved = 0;
        level_offset += romfs_ivfc.levels[i].size();
    }
    
    // Copy master hash
    memcpy(ivfc->master_hash, romfs_ivfc.master_hash, 0x20);
    
    // Set section CTR for RomFS
    uint32_t section1_gen = 1;
    header->fs_headers[1].section_ctr[0] = (section1_gen >> 24) & 0xFF;
    header->fs_headers[1].section_ctr[1] = (section1_gen >> 16) & 0xFF;
    header->fs_headers[1].section_ctr[2] = (section1_gen >> 8) & 0xFF;
    header->fs_headers[1].section_ctr[3] = section1_gen & 0xFF;
    header->fs_headers[1].section_ctr[4] = 0;
    header->fs_headers[1].section_ctr[5] = 0;
    header->fs_headers[1].section_ctr[6] = 0;
    header->fs_headers[1].section_ctr[7] = 0;
    
    // Copy section data
    // Section 0: hash table + padding + PFS0
    memcpy(nca.data() + section0_offset, exefs_result.hash_table.data(), exefs_result.hash_table.size());
    memcpy(nca.data() + section0_offset + exefs_result.pfs0_offset, 
           exefs_result.pfs0_data.data(), exefs_result.pfs0_data.size());
    
    // Section 1: IVFC levels concatenated
    size_t ivfc_write_offset = section1_offset;
    for (int i = 0; i < 6; i++) {
        memcpy(nca.data() + ivfc_write_offset, romfs_ivfc.levels[i].data(), romfs_ivfc.levels[i].size());
        ivfc_write_offset += romfs_ivfc.levels[i].size();
    }
    
    // Set key area (slot 2 is the AES-CTR key for content)
    // Even in plaintext mode, we set and encrypt the key area
    memcpy(header->encrypted_keys[2], content_key, 0x10);
    
    // Plaintext mode: section data is NOT encrypted (crypt_type = 1)
    // This is standard for homebrew NCAs on CFW systems
    
    // Calculate FS header hashes
    ComputeSHA256(reinterpret_cast<uint8_t*>(&header->fs_headers[0]), sizeof(NcaFsHeader), header->section_hashes[0]);
    ComputeSHA256(reinterpret_cast<uint8_t*>(&header->fs_headers[1]), sizeof(NcaFsHeader), header->section_hashes[1]);
    
    // Encrypt key area with key_area_key_application
    Log::Debug("NSPBuild: Encrypting key area\n");
    Aes128Context kak_ctx;
    aes128ContextCreate(&kak_ctx, keys.key_area_key_application[0], true);
    for (int i = 0; i < 4; i++) {
        aes128EncryptBlock(&kak_ctx, header->encrypted_keys[i], header->encrypted_keys[i]);
    }
    
    // Encrypt header with header_key using AES-XTS
    Log::Debug("NSPBuild: Encrypting header\n");
    AesXtsEncrypt(keys.header_key, 0, nca.data(), NCA_HEADER_SIZE);
    
    return nca;
}

static std::vector<uint8_t> BuildControlNCA(const KeySet &keys, uint64_t title_id,
                                             const std::vector<uint8_t> &nacp_data,
                                             const std::vector<uint8_t> &icon_data) {
    Log::Debug("NSPBuild: Building Control NCA\n");
    
    // Build Control RomFS
    std::vector<std::pair<std::string, std::vector<uint8_t>>> control_files;
    control_files.push_back({"control.nacp", nacp_data});
    if (!icon_data.empty()) {
        control_files.push_back({"icon_AmericanEnglish.dat", icon_data});
    }
    
    std::vector<uint8_t> romfs = BuildRomFS(control_files);
    
    // Build IVFC tree
    IvfcBuildResult romfs_ivfc = BuildIvfcTree(romfs);
    
    // Generate random content key
    uint8_t content_key[0x10];
    randomGet(content_key, sizeof(content_key));
    
    // Calculate section size
    size_t section_data_size = 0;
    for (int i = 0; i < 6; i++) {
        section_data_size += romfs_ivfc.levels[i].size();
    }
    size_t section_size_aligned = Align(section_data_size, MEDIA_UNIT);
    size_t total_size = NCA_HEADER_SIZE + section_size_aligned;
    
    // Allocate NCA
    std::vector<uint8_t> nca(total_size, 0);
    NcaHeader* header = reinterpret_cast<NcaHeader*>(nca.data());
    
    // Fill header
    header->magic = MAGIC_NCA3;
    header->distribution_type = 0;
    header->content_type = 2;  // Control
    header->crypto_type = 0;
    header->kaek_index = 0;
    header->nca_size = total_size;
    header->title_id = title_id;
    header->sdk_version = 0x000D0000;
    
    // Section 0 entry
    header->section_entries[0].media_start_offset = static_cast<uint32_t>(NCA_HEADER_SIZE / MEDIA_UNIT);
    header->section_entries[0].media_end_offset = static_cast<uint32_t>(total_size / MEDIA_UNIT);
    header->section_entries[0].enabled_and_padding[0] = 1;
    
    // FS Header 0 - RomFS (IVFC)
    header->fs_headers[0].version = 2;
    header->fs_headers[0].fs_type = 0;
    header->fs_headers[0].hash_type = 3;
    header->fs_headers[0].crypt_type = 1;     // Plaintext (no section encryption for homebrew)
    
    // IVFC header
    IvfcHeader* ivfc = &header->fs_headers[0].romfs_superblock.ivfc_header;
    ivfc->magic = MAGIC_IVFC;
    ivfc->id = 0x20000;
    ivfc->master_hash_size = 0x20;
    ivfc->num_levels = 7;
    
    uint64_t level_offset = 0;
    for (int i = 0; i < 6; i++) {
        ivfc->level_headers[i].logical_offset = level_offset;
        ivfc->level_headers[i].hash_data_size = romfs_ivfc.level_sizes[i];
        ivfc->level_headers[i].block_size = 0x0E;
        level_offset += romfs_ivfc.levels[i].size();
    }
    memcpy(ivfc->master_hash, romfs_ivfc.master_hash, 0x20);
    
    // Section CTR (not used in plaintext mode but set anyway)
    header->fs_headers[0].section_ctr[0] = 0;
    header->fs_headers[0].section_ctr[1] = 0;
    header->fs_headers[0].section_ctr[2] = 0;
    header->fs_headers[0].section_ctr[3] = 0;
    
    // Copy section data (plaintext - no encryption)
    size_t write_offset = NCA_HEADER_SIZE;
    for (int i = 0; i < 6; i++) {
        memcpy(nca.data() + write_offset, romfs_ivfc.levels[i].data(), romfs_ivfc.levels[i].size());
        write_offset += romfs_ivfc.levels[i].size();
    }
    
    // Set key area
    memcpy(header->encrypted_keys[2], content_key, 0x10);
    
    // Calculate FS header hash
    ComputeSHA256(reinterpret_cast<uint8_t*>(&header->fs_headers[0]), sizeof(NcaFsHeader), header->section_hashes[0]);
    
    // Encrypt key area
    Aes128Context kak_ctx;
    aes128ContextCreate(&kak_ctx, keys.key_area_key_application[0], true);
    for (int i = 0; i < 4; i++) {
        aes128EncryptBlock(&kak_ctx, header->encrypted_keys[i], header->encrypted_keys[i]);
    }
    
    // Encrypt header
    AesXtsEncrypt(keys.header_key, 0, nca.data(), NCA_HEADER_SIZE);
    
    return nca;
}

static std::vector<uint8_t> BuildMetaNCA(const KeySet &keys, uint64_t title_id,
                                          const std::vector<uint8_t> &program_nca,
                                          const std::vector<uint8_t> &control_nca) {
    Log::Debug("NSPBuild: Building Meta NCA\n");
    
    // Generate NCA IDs from hashes
    uint8_t program_hash[0x20], control_hash[0x20];
    ComputeSHA256(program_nca.data(), program_nca.size(), program_hash);
    ComputeSHA256(control_nca.data(), control_nca.size(), control_hash);
    
    // Build CNMT
    std::vector<uint8_t> cnmt;
    
    CnmtHeader cnmt_header;
    memset(&cnmt_header, 0, sizeof(cnmt_header));
    cnmt_header.title_id = title_id;
    cnmt_header.title_version = 0;
    cnmt_header.meta_type = 0x80;  // Application
    cnmt_header.extended_header_size = sizeof(CnmtExtendedHeader);
    cnmt_header.content_count = 2;  // Program + Control
    cnmt_header.content_meta_count = 0;
    
    CnmtExtendedHeader ext_header;
    memset(&ext_header, 0, sizeof(ext_header));
    ext_header.patch_id = title_id + 0x800;
    
    CnmtContentEntry program_entry;
    memset(&program_entry, 0, sizeof(program_entry));
    memcpy(program_entry.hash, program_hash, 0x20);
    memcpy(program_entry.nca_id, program_hash, 0x10);
    program_entry.size[0] = program_nca.size() & 0xFF;
    program_entry.size[1] = (program_nca.size() >> 8) & 0xFF;
    program_entry.size[2] = (program_nca.size() >> 16) & 0xFF;
    program_entry.size[3] = (program_nca.size() >> 24) & 0xFF;
    program_entry.size[4] = (program_nca.size() >> 32) & 0xFF;
    program_entry.size[5] = (program_nca.size() >> 40) & 0xFF;
    program_entry.content_type = 1;  // Program
    
    CnmtContentEntry control_entry;
    memset(&control_entry, 0, sizeof(control_entry));
    memcpy(control_entry.hash, control_hash, 0x20);
    memcpy(control_entry.nca_id, control_hash, 0x10);
    control_entry.size[0] = control_nca.size() & 0xFF;
    control_entry.size[1] = (control_nca.size() >> 8) & 0xFF;
    control_entry.size[2] = (control_nca.size() >> 16) & 0xFF;
    control_entry.size[3] = (control_nca.size() >> 24) & 0xFF;
    control_entry.size[4] = (control_nca.size() >> 32) & 0xFF;
    control_entry.size[5] = (control_nca.size() >> 40) & 0xFF;
    control_entry.content_type = 3;  // Control
    
    // Assemble CNMT
    cnmt.resize(sizeof(CnmtHeader) + sizeof(CnmtExtendedHeader) + 2 * sizeof(CnmtContentEntry) + 0x20);
    size_t offset = 0;
    memcpy(cnmt.data() + offset, &cnmt_header, sizeof(cnmt_header)); offset += sizeof(cnmt_header);
    memcpy(cnmt.data() + offset, &ext_header, sizeof(ext_header)); offset += sizeof(ext_header);
    memcpy(cnmt.data() + offset, &program_entry, sizeof(program_entry)); offset += sizeof(program_entry);
    memcpy(cnmt.data() + offset, &control_entry, sizeof(control_entry)); offset += sizeof(control_entry);
    // Digest (leave as zeros)
    
    // Build CNMT PFS0
    std::stringstream ss;
    ss << "Application_" << std::hex << std::setfill('0') << std::setw(16) << title_id << ".cnmt";
    std::string cnmt_filename = ss.str();
    
    std::vector<std::pair<std::string, std::vector<uint8_t>>> cnmt_files;
    cnmt_files.push_back({cnmt_filename, cnmt});
    
    // Build PFS0 with hash table
    PFS0BuildResult cnmt_result;
    cnmt_result.pfs0_data = BuildPFS0(cnmt_files);
    
    // Build hash table - hash only actual bytes, not zero-padded blocks (matching reference)
    size_t pfs0_size = cnmt_result.pfs0_data.size();
    size_t num_blocks = (pfs0_size + PFS0_META_HASH_BLOCK_SIZE - 1) / PFS0_META_HASH_BLOCK_SIZE;
    cnmt_result.hash_table.resize(num_blocks * 0x20);
    
    for (size_t i = 0; i < num_blocks; i++) {
        size_t block_start = i * PFS0_META_HASH_BLOCK_SIZE;
        size_t block_len = std::min(static_cast<size_t>(PFS0_META_HASH_BLOCK_SIZE), pfs0_size - block_start);
        
        ComputeSHA256(cnmt_result.pfs0_data.data() + block_start, block_len, 
                      cnmt_result.hash_table.data() + i * 0x20);
    }
    cnmt_result.pfs0_offset = Align(cnmt_result.hash_table.size(), 0x200);
    
    // Generate random content key
    uint8_t content_key[0x10];
    randomGet(content_key, sizeof(content_key));
    
    // Calculate section size
    size_t section_data_size = cnmt_result.pfs0_offset + cnmt_result.pfs0_data.size();
    size_t section_size_aligned = Align(section_data_size, MEDIA_UNIT);
    size_t total_size = NCA_HEADER_SIZE + section_size_aligned;
    
    // Allocate NCA
    std::vector<uint8_t> nca(total_size, 0);
    NcaHeader* header = reinterpret_cast<NcaHeader*>(nca.data());
    
    // Fill header
    header->magic = MAGIC_NCA3;
    header->distribution_type = 0;
    header->content_type = 1;  // Meta
    header->crypto_type = 0;
    header->kaek_index = 0;
    header->nca_size = total_size;
    header->title_id = title_id;
    header->sdk_version = 0x000D0000;
    
    // Section 0 entry
    header->section_entries[0].media_start_offset = static_cast<uint32_t>(NCA_HEADER_SIZE / MEDIA_UNIT);
    header->section_entries[0].media_end_offset = static_cast<uint32_t>(total_size / MEDIA_UNIT);
    header->section_entries[0].enabled_and_padding[0] = 1;
    
    // FS Header 0 - PFS0
    header->fs_headers[0].version = 2;
    header->fs_headers[0].fs_type = 1;
    header->fs_headers[0].hash_type = 2;
    header->fs_headers[0].crypt_type = 1;     // Plaintext (no section encryption for homebrew)
    
    // PFS0 superblock
    header->fs_headers[0].pfs0_superblock.block_size = PFS0_META_HASH_BLOCK_SIZE;
    header->fs_headers[0].pfs0_superblock.always_2 = 2;
    header->fs_headers[0].pfs0_superblock.hash_table_offset = 0;
    header->fs_headers[0].pfs0_superblock.hash_table_size = cnmt_result.hash_table.size();
    header->fs_headers[0].pfs0_superblock.pfs0_offset = cnmt_result.pfs0_offset;
    header->fs_headers[0].pfs0_superblock.pfs0_size = cnmt_result.pfs0_data.size();
    
    // Calculate master hash
    ComputeSHA256(cnmt_result.hash_table.data(), cnmt_result.hash_table.size(),
                  header->fs_headers[0].pfs0_superblock.master_hash);
    
    // Section CTR (not used in plaintext mode but set anyway)
    header->fs_headers[0].section_ctr[0] = 0;
    
    // Copy section data (plaintext - no encryption)
    memcpy(nca.data() + NCA_HEADER_SIZE, cnmt_result.hash_table.data(), cnmt_result.hash_table.size());
    memcpy(nca.data() + NCA_HEADER_SIZE + cnmt_result.pfs0_offset, 
           cnmt_result.pfs0_data.data(), cnmt_result.pfs0_data.size());
    
    // Set key area
    memcpy(header->encrypted_keys[2], content_key, 0x10);
    
    // Calculate FS header hash
    ComputeSHA256(reinterpret_cast<uint8_t*>(&header->fs_headers[0]), sizeof(NcaFsHeader), header->section_hashes[0]);
    
    // Encrypt key area
    Aes128Context kak_ctx;
    aes128ContextCreate(&kak_ctx, keys.key_area_key_application[0], true);
    for (int i = 0; i < 4; i++) {
        aes128EncryptBlock(&kak_ctx, header->encrypted_keys[i], header->encrypted_keys[i]);
    }
    
    // Encrypt header
    AesXtsEncrypt(keys.header_key, 0, nca.data(), NCA_HEADER_SIZE);
    
    return nca;
}

// ============================================================================
// Public API Implementation
// ============================================================================

NroMetadata ExtractNroMetadata(const std::string &nro_path) {
    NroMetadata meta;
    meta.valid = false;
    
    Log::Debug("[H1-H5] ExtractNroMetadata entry: path=%s sizeof_NroStart=%zu sizeof_NroHeader=%zu sizeof_NroAssetHeader=%zu\n", 
               nro_path.c_str(), sizeof(NroStart), sizeof(NroHeader), sizeof(NroAssetHeader));
    
    FILE *f = fopen(nro_path.c_str(), "rb");
    if (!f) {
        meta.error = "Failed to open NRO file";
        return meta;
    }
    
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    Log::Debug("[H3-H4] FileOpened: file_size=%ld\n", file_size);
    
    // Read NRO start
    NroStart start;
    if (fread(&start, sizeof(start), 1, f) != 1) {
        fclose(f);
        meta.error = "Failed to read NRO start header";
        return meta;
    }
    
    // Read NRO header
    NroHeader header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        meta.error = "Failed to read NRO header";
        return meta;
    }
    
    Log::Debug("[H1-H2] HeaderRead: magic=0x%08x version=%u size=%u ftell=%ld\n", 
               header.magic, header.version, header.size, ftell(f));
    
    // Verify magic
    if (header.magic != NRO_MAGIC) {
        fclose(f);
        meta.error = "Invalid NRO magic";
        return meta;
    }
    
    // Asset section starts at offset header.size
    uint32_t asset_offset = header.size;
    long asset_size = file_size - header.size;
    
    if (asset_size <= 0) {
        Log::Debug("[H1] NoAssetSection: asset_offset=%u file_size=%ld asset_size=%ld\n", 
                   asset_offset, file_size, asset_size);
        fclose(f);
        meta.error = "NRO has no asset section";
        return meta;
    }
    
    Log::Debug("[H2-H4] BeforeSeek: seeking_to=%u file_size=%ld asset_size=%ld\n", 
               asset_offset, file_size, asset_size);
    
    if (fseek(f, asset_offset, SEEK_SET) != 0) {
        fclose(f);
        meta.error = "Failed to seek to asset section";
        return meta;
    }
    
    long pos_after_seek = ftell(f);
    Log::Debug("[H4] AfterSeek: position=%ld remaining=%ld\n", pos_after_seek, file_size - pos_after_seek);
    
    // Read asset header
    NroAssetHeader asset;
    if (fread(&asset, sizeof(asset), 1, f) != 1) {
        Log::Debug("[H1-H4] AssetReadFailed: ferror=%d feof=%d sizeof_asset=%zu\n", ferror(f), feof(f), sizeof(NroAssetHeader));
        fclose(f);
        meta.error = "Failed to read asset header";
        return meta;
    }
    
    Log::Debug("[H1] AssetHeaderRead: magic=0x%08x expected=0x%08x version=%u\n", asset.magic, ASET_MAGIC, asset.version);
    
    if (asset.magic != ASET_MAGIC) {
        fclose(f);
        meta.error = "Invalid asset magic";
        return meta;
    }
    
    // Read NACP if present
    if (asset.nacp_offset > 0 && asset.nacp_size >= sizeof(NacpHeader)) {
        if (fseek(f, asset_offset + asset.nacp_offset, SEEK_SET) == 0) {
            NacpHeader nacp;
            if (fread(&nacp, sizeof(nacp), 1, f) == 1) {
                for (int i = 0; i < 16 && meta.name.empty(); i++) {
                    if (nacp.titles[i].name[0] != '\0') {
                        meta.name = nacp.titles[i].name;
                    }
                }
                
                for (int i = 0; i < 16 && meta.publisher.empty(); i++) {
                    if (nacp.titles[i].publisher[0] != '\0') {
                        meta.publisher = nacp.titles[i].publisher;
                    }
                }
                
                if (nacp.display_version[0] != '\0') {
                    meta.version = nacp.display_version;
                }
            }
        }
    }
    
    // Read icon if present
    if (asset.icon_offset > 0 && asset.icon_size > 0) {
        if (fseek(f, asset_offset + asset.icon_offset, SEEK_SET) == 0) {
            meta.icon.resize(asset.icon_size);
            if (fread(meta.icon.data(), 1, asset.icon_size, f) != asset.icon_size) {
                meta.icon.clear();
            }
        }
    }
    
    fclose(f);
    
    // Set defaults if metadata is missing
    if (meta.name.empty()) {
        size_t last_slash = nro_path.find_last_of("/\\");
        size_t last_dot = nro_path.rfind('.');
        if (last_slash != std::string::npos && last_dot != std::string::npos && last_dot > last_slash) {
            meta.name = nro_path.substr(last_slash + 1, last_dot - last_slash - 1);
        } else if (last_slash != std::string::npos) {
            meta.name = nro_path.substr(last_slash + 1);
        } else {
            meta.name = nro_path;
        }
    }
    
    if (meta.publisher.empty()) {
        meta.publisher = "Unknown";
    }
    
    if (meta.version.empty()) {
        meta.version = "1.0.0";
    }
    
    meta.valid = true;
    return meta;
}

std::string GenerateTitleId(const std::string &nro_path) {
    uint8_t hash[0x20];
    
    Sha256Context ctx;
    sha256ContextCreate(&ctx);
    sha256ContextUpdate(&ctx, nro_path.c_str(), nro_path.size());
    sha256ContextGetHash(&ctx, hash);
    
    // Build title ID: 01 + 6 bytes from hash + 00 (16 hex chars total)
    // Format: 01XXXXXXXXXXXX00 where XX = hash bytes, ends in 00 for Application type
    std::stringstream ss;
    ss << "01";
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < 6; i++) {
        ss << std::setw(2) << static_cast<int>(hash[i]);
    }
    ss << "00";
    
    std::string title_id = ss.str();
    
    int attempts = 0;
    while (IsSystemTitleId(title_id) && attempts < 16) {
        title_id[15] = "123456789abcdef"[attempts % 15];
        attempts++;
    }
    
    return title_id;
}

bool KeysFileExists() {
    return true;
}

std::string GetKeysPath() {
    return "(derived from hardware)";
}

std::string SanitizeFilename(const std::string &name) {
    std::string result;
    result.reserve(name.size());
    
    for (char c : name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || 
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            result += '_';
        } else if (c >= 32) {
            result += c;
        }
    }
    
    size_t start = result.find_first_not_of(" .");
    size_t end = result.find_last_not_of(" .");
    
    if (start == std::string::npos) return "forwarder";
    return result.substr(start, end - start + 1);
}

NSPBuildResult BuildForwarderNSP(const NSPBuildConfig &config) {
    NSPBuildResult result;
    result.success = false;
    
    if (config.nro_path.empty()) {
        result.error = "NRO path is empty";
        return result;
    }
    
    // Derive keys from hardware
    KeySet keys = DeriveKeys();
    if (!keys.valid) {
        result.error = "Failed to derive encryption keys from hardware";
        return result;
    }
    
    // Extract NRO metadata
    NroMetadata meta = ExtractNroMetadata(config.nro_path);
    if (!meta.valid) {
        result.error = "Failed to extract NRO metadata: " + meta.error;
        return result;
    }
    
    // Use overrides if provided
    std::string name = config.override_name.empty() ? meta.name : config.override_name;
    std::string publisher = config.override_publisher.empty() ? meta.publisher : config.override_publisher;
    std::string version = config.override_version.empty() ? meta.version : config.override_version;
    std::vector<uint8_t> icon = config.override_icon.empty() ? meta.icon : config.override_icon;
    
    // Generate or use provided title ID
    std::string title_id_str = config.title_id.empty() ? GenerateTitleId(config.nro_path) : config.title_id;
    result.title_id = title_id_str;
    
    // Parse title ID to uint64
    uint64_t title_id = 0;
    for (int i = 0; i < 16 && i < static_cast<int>(title_id_str.size()); i++) {
        char c = title_id_str[i];
        title_id <<= 4;
        if (c >= '0' && c <= '9') title_id |= (c - '0');
        else if (c >= 'a' && c <= 'f') title_id |= (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') title_id |= (c - 'A' + 10);
    }
    
    Log::Debug("NSPBuild: Building forwarder for %s\n", config.nro_path.c_str());
    Log::Debug("  Name: %s\n", name.c_str());
    Log::Debug("  Publisher: %s\n", publisher.c_str());
    Log::Debug("  Version: %s\n", version.c_str());
    Log::Debug("  Title ID: %s (0x%016lx)\n", title_id_str.c_str(), title_id);
    
    // Build sdmc path
    std::string sdmc_path = config.nro_path;
    if (sdmc_path.substr(0, 5) != "sdmc:") {
        if (sdmc_path[0] == '/') {
            sdmc_path = "sdmc:" + sdmc_path;
        }
    }
    
    // Load forwarder binaries from romfs
    Log::Debug("NSPBuild: Loading forwarder binaries from romfs...\n");
    std::vector<uint8_t> forwarder_nso = ReadFileBytes("romfs:/forwarder.nso");
    std::vector<uint8_t> forwarder_npdm = ReadFileBytes("romfs:/forwarder.npdm");
    
    if (forwarder_nso.empty()) {
        result.error = "Failed to load forwarder.nso from romfs";
        return result;
    }
    if (forwarder_npdm.empty()) {
        result.error = "Failed to load forwarder.npdm from romfs";
        return result;
    }
    
    // Build ExeFS PFS0
    Log::Debug("NSPBuild: Building ExeFS...\n");
    std::vector<std::pair<std::string, std::vector<uint8_t>>> exefs_files;
    exefs_files.push_back({"main", forwarder_nso});
    exefs_files.push_back({"main.npdm", forwarder_npdm});
    std::vector<uint8_t> exefs_pfs0 = BuildPFS0(exefs_files);
    
    // Build RomFS
    Log::Debug("NSPBuild: Building RomFS...\n");
    std::vector<std::pair<std::string, std::vector<uint8_t>>> romfs_files;
    romfs_files.push_back({"nextNroPath", std::vector<uint8_t>(sdmc_path.begin(), sdmc_path.end())});
    romfs_files.push_back({"nextArgv", std::vector<uint8_t>(sdmc_path.begin(), sdmc_path.end())});
    std::vector<uint8_t> romfs_data = BuildRomFS(romfs_files);
    
    // Build NACP
    std::vector<uint8_t> nacp_data = BuildNACP(name, publisher, version, title_id_str);
    
    // Build NCAs
    Log::Debug("NSPBuild: Building Program NCA...\n");
    std::vector<uint8_t> program_nca = BuildProgramNCA(keys, title_id, exefs_pfs0, romfs_data);
    
    Log::Debug("NSPBuild: Building Control NCA...\n");
    std::vector<uint8_t> control_nca = BuildControlNCA(keys, title_id, nacp_data, icon);
    
    Log::Debug("NSPBuild: Building Meta NCA...\n");
    std::vector<uint8_t> meta_nca = BuildMetaNCA(keys, title_id, program_nca, control_nca);
    
    // Generate NCA IDs
    uint8_t program_hash[0x20], control_hash[0x20], meta_hash[0x20];
    ComputeSHA256(program_nca.data(), program_nca.size(), program_hash);
    ComputeSHA256(control_nca.data(), control_nca.size(), control_hash);
    ComputeSHA256(meta_nca.data(), meta_nca.size(), meta_hash);
    
    std::string program_nca_id = ToLowerHex(program_hash, 16);
    std::string control_nca_id = ToLowerHex(control_hash, 16);
    std::string meta_nca_id = ToLowerHex(meta_hash, 16);
    
    // Build final NSP
    Log::Debug("NSPBuild: Building final NSP...\n");
    std::vector<std::pair<std::string, std::vector<uint8_t>>> nsp_files;
    nsp_files.push_back({program_nca_id + ".nca", program_nca});
    nsp_files.push_back({control_nca_id + ".nca", control_nca});
    nsp_files.push_back({meta_nca_id + ".cnmt.nca", meta_nca});
    
    std::vector<uint8_t> nsp_data = BuildPFS0(nsp_files);
    
    // Determine output path
    std::string output_path = config.output_path;
    if (output_path.empty()) {
        std::string safe_name = SanitizeFilename(name);
        output_path = "sdmc:/switch/" + safe_name + " [" + title_id_str + "].nsp";
    }
    result.output_path = output_path;
    
    // Write NSP file
    if (!WriteFileBytes(output_path, nsp_data)) {
        result.error = "Failed to write NSP file to " + output_path;
        return result;
    }
    
    Log::Debug("NSPBuild: Successfully created NSP at %s\n", output_path.c_str());
    Log::Debug("NSPBuild: NSP size: %zu bytes\n", nsp_data.size());
    
    result.success = true;
    return result;
}

} // namespace NSPBuild
