#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace NSPBuild {

// ============================================================================
// NRO Format Structures (for parsing metadata)
// ============================================================================

// NRO Start header (first 16 bytes)
struct NroStart {
    uint32_t unused;
    uint32_t mod_offset;
    uint8_t padding[8];
};

// NRO Header (at offset 0x10, size 0x50 = 80 bytes)
// Note: Asset section (ASET) is located at file offset `size`, NOT via separate fields
struct NroHeader {
    uint32_t magic;          // "NRO0"
    uint32_t version;
    uint32_t size;           // Size of NRO code/data (asset section starts here)
    uint32_t flags;
    uint32_t text_offset;
    uint32_t text_size;
    uint32_t ro_offset;
    uint32_t ro_size;
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t bss_size;
    uint32_t reserved;
    uint8_t build_id[0x20];
    // Asset section (ASET header) is at file offset `size`
};

// NRO Asset Header (at asset_offset)
struct NroAssetHeader {
    uint32_t magic;          // "ASET"
    uint32_t version;
    uint64_t icon_offset;    // Offset relative to asset header
    uint64_t icon_size;
    uint64_t nacp_offset;    // Offset relative to asset header
    uint64_t nacp_size;
    uint64_t romfs_offset;
    uint64_t romfs_size;
};

// NACP (Nintendo Application Control Property) structure - simplified for reading
// Full structure is 0x4000 bytes
struct NacpTitle {
    char name[0x200];        // Application name
    char publisher[0x100];   // Publisher name
};

struct NacpHeader {
    NacpTitle titles[16];    // One per language (0x3000 bytes)
    char isbn[0x25];
    uint8_t startup_user_account;
    uint8_t user_account_switch_lock;
    uint8_t add_on_content_registration_type;
    uint32_t attribute;
    uint32_t supported_language;
    uint32_t parental_control;
    uint8_t screenshot;
    uint8_t video_capture;
    uint8_t data_loss_confirmation;
    uint8_t play_log_policy;
    uint64_t presence_group_id;
    int8_t rating_age[0x20];
    char display_version[0x10];
    uint64_t add_on_content_base_id;
    uint64_t save_data_owner_id;
    int64_t user_account_save_data_size;
    int64_t user_account_save_data_journal_size;
    int64_t device_save_data_size;
    int64_t device_save_data_journal_size;
    int64_t bcat_delivery_cache_storage_size;
    char application_error_code_category[8];
    uint64_t local_communication_id[8];
    uint8_t logo_type;
    uint8_t logo_handling;
    uint8_t runtime_add_on_content_install;
    uint8_t runtime_parameter_delivery;
    uint8_t reserved30f4[2];
    uint8_t crash_report;
    uint8_t hdcp;
    uint64_t seed_for_pseudo_device_id;
    char bcat_passphrase[0x41];
    uint8_t startup_user_account_option;
    uint8_t reserved3148[6];
    int64_t user_account_save_data_size_max;
    int64_t user_account_save_data_journal_size_max;
    int64_t device_save_data_size_max;
    int64_t device_save_data_journal_size_max;
    int64_t temporary_storage_size;
    int64_t cache_storage_size;
    int64_t cache_storage_journal_size;
    int64_t cache_storage_data_and_journal_size_max;
    uint16_t cache_storage_index_max;
    uint8_t reserved318a[6];
    uint64_t play_log_queryable_application_id[16];
    uint8_t play_log_query_capability;
    uint8_t repair_flag;
    uint8_t program_index;
    uint8_t required_network_service_license_on_launch;
    uint8_t reserved3214[0xDEC];
};

// ============================================================================
// NRO Metadata extracted from the file
// ============================================================================

struct NroMetadata {
    std::string name;
    std::string publisher;
    std::string version;
    std::vector<uint8_t> icon;  // JPEG data (256x256)
    bool valid = false;
    std::string error;
};

// ============================================================================
// NSP Build Configuration
// ============================================================================

struct NSPBuildConfig {
    std::string nro_path;           // Full path to NRO (e.g., "sdmc:/switch/app.nro")
    std::string output_path;        // Where to save the NSP
    std::string title_id;           // 16-char hex title ID (generated from path hash if empty)
    
    // Optional overrides (empty = use NRO metadata)
    std::string override_name;
    std::string override_publisher;
    std::string override_version;
    std::vector<uint8_t> override_icon;
};

struct NSPBuildResult {
    bool success = false;
    std::string error;
    std::string output_path;
    std::string title_id;
};

// ============================================================================
// Public API
// ============================================================================

// Extract metadata from an NRO file
NroMetadata ExtractNroMetadata(const std::string &nro_path);

// Generate a title ID from the NRO path (using SHA256 hash)
std::string GenerateTitleId(const std::string &nro_path);

// Check if keys file exists (sdmc:/switch/prod.keys)
bool KeysFileExists();

// Get the path to the keys file
std::string GetKeysPath();

// Build an NSP forwarder from an NRO
NSPBuildResult BuildForwarderNSP(const NSPBuildConfig &config);

// Get a safe filename for the NSP (removes invalid characters)
std::string SanitizeFilename(const std::string &name);

} // namespace NSPBuild
