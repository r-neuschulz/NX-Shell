#pragma once

namespace Lang {
    typedef enum {
        // Prompt/Message buttons
        ButtonOK = 0,
        ButtonCancel,

        // Options dialog
        OptionsTitle,
        OptionsSelectAll,
        OptionsClearAll,
        OptionsProperties,
        OptionsRename,
        OptionsNewFolder,
        OptionsNewFile,
        OptionsCopy,
        OptionsRecursiveCopyError,
        OptionsMove,
        OptionsPaste,
        OptionsDelete,
        OptionsSetArchiveBit,
        OptionsRenamePrompt,
        OptionsFolderPrompt,
        OptionsFilePrompt,
        OptionsCopying,

        // Properties dialog
        PropertiesName,
        PropertiesSize,
        PropertiesCreated,
        PropertiesModified,
        PropertiesAccessed,
        PropertiesWidth,
        PropertiesHeight,

        // Delete dialog
        DeleteMessage,
        DeleteMultiplePrompt,
        DeletePrompt,

        // Archive dialog
        ArchiveTitle,
        ArchiveMessage,
        ArchivePrompt,
        ArchiveExtracting,

        // SettingsWindow
        SettingsTitle,
        SettingsSortTitle,
        SettingsLanguageTitle,
        SettingsUSBTitle,
        SettingsUSBUnmount,
        SettingsImageViewTitle,
        SettingsDevOptsTitle,
        SettingsResolutionTitle,
        SettingsResolutionAuto,
        SettingsResolution1080p,
        SettingsResolution720p,
        SettingsAboutTitle,
        SettingsCheckForUpdates,
        SettingsImageViewFilenameToggle,
        SettingsImageViewFullscreenToggle,
        SettingsDevOptsLogsToggle,
        SettingsAboutVersion,
        SettingsAboutAuthor,
        SettingsAboutBanner,
        SettingsAboutLicense,

        // Stats for nerds
        SettingsStatsTitle,
        SettingsStatsToggle,
        
        // Stats overlay strings
        StatsResolution,
        StatsFPS,
        StatsCPU,
        StatsGPU,
        StatsMemory,
        StatsSOCTemp,
        StatsSkinTemp,
        StatsNA,

        // Accent Color
        SettingsAccentColorTitle,
        SettingsAccentColorReset,

        // Theme
        SettingsThemeTitle,
        SettingsThemeAuto,
        SettingsThemeDark,
        SettingsThemeLight,

        // Updates Dialog
        UpdateTitle,
        UpdateNetworkError,
        UpdateAvailable,
        UpdatePrompt,
        UpdateSuccess,
        UpdateRestart,
        UpdateNotAvailable,

        // USB Dialog
        USBUnmountPrompt,
        USBUnmountSuccess,

        // Keyboard
        KeyboardEmpty,

        // Button Hints
        HintOpen,
        HintBack,
        HintSelect,
        HintOptions,
        HintDrive,
        HintExit,

        // File Browser
        FileBrowserFilename,
        FileBrowserDevice,
        FileBrowserSize,
        FileBrowserModified,
        FileBrowserArchive,
        FileBrowserSelectDevice,

        // Button Hints (continued)
        HintDetails,
        HintConfirm,
        HintCancel,

    // Tabs
    TabFiles,
    TabSettings,
    TabAbout,

    // Image/Text Viewer Hints
    HintPrev,
    HintNext,
    HintZoomIn,
    HintZoomOut,
    HintProperties,
    HintFullscreen,
    HintExitFullscreen,

    // Reset Settings
    SettingsResetTitle,
    SettingsResetMessage,
    SettingsResetButton,

    // Replace Confirmation
    ReplaceTitle,
    ReplaceMessage,
    ReplaceButton,

    // Multi-file Replace Confirmation
    MultiReplaceMessage,
    MultiReplaceAll,
    MultiReplaceSkip,

    // Hex Mode
    HintHexMode,
    HexModeOpenTitle,
    HexModeOpenMessage,
    HexModeOpenAsText,
    HexModeOpenAsHex,

    // Button Style
    SettingsButtonStyleTitle,
    SettingsButtonStyleColored,
    SettingsButtonStyleMono,
    SettingsButtonStyleAccent,

    // Max
    Max
    } StringID;
}

extern const char **strings[Lang::Max];
