#include <switch.h>

#include "language.hpp"

// Japanese (need to check and verify)
static const char *strings_jp[] {
    "OK",
    "キャンセル",

    "オプション",
    "すべて選択",
    "すべてクリア",
    "プロパティ",
    "名前変更",
    "新規フォルダ",
    "新規ファイル",
    "コピー",
    "フォルダをそれ自体にコピーすることはできません。",
    "移動",
    "貼り付け",
    "削除",
    "アーカイブビットを設定",
    "名前を入力",
    "フォルダ名を入力",
    "ファイル名を入力",
    "コピー中:",

    "名前: ",
    "サイズ: ",
    "作成日時: ",
    "更新日時: ",
    "アクセス日時: ",
    "幅: ",
    "高さ: ",

    "この操作は元に戻せません。",
    "以下を削除しますか:",
    "削除しますか ",

    "アーカイブを展開",
    "この操作には時間がかかる場合があります。",
    "展開しますか ",
    "展開中:",

    "設定",
    "並べ替え設定",
    "言語",
    "USB",
    "USBデバイスを取り外す",
    "画像ビューア",
    "開発者オプション",
    "表示解像度",
    " 自動",
    " 1080p",
    " 720p",
    "このアプリについて",
    "アップデート",
    "サードパーティコンポーネント",
    "アップデートを確認",
    " ファイル名を表示",
    " 全画面で画像を開く",
    " ログを有効にする",
    "バージョン",
    "作者",
    "バナー",
    "ライセンス",

    "詳細統計",
    " パフォーマンスオーバーレイを表示",

    // Stats overlay strings
    "解像度: %dx%d",
    "FPS: %.1f (%.2fms)",
    "CPU: %u MHz",
    "GPU: %u MHz",
    "RAM: %.1f/%.1f MB",
    "SOC温度: %.1f°C",
    "外装温度: %.1f°C",
    "N/A",

    "アクセントカラー",
    "リセット",

    "テーマ",
    "自動",
    "ダーク",
    "ライト",

    "アップデート",
    "ネットワークに接続できませんでした。",
    "アップデートが利用可能です。",
    "NX-Shell バージョン をダウンロードしてインストールしますか ",
    "アップデートが成功しました。",
    "アプリケーションを再起動してください。",
    "最新バージョンです。",

    "接続されているすべてのUSBデバイスを取り外しますか？",
    "USBデバイスを安全に取り外すことができます。",

    "名前を空にすることはできません。",

    "開く",
    "戻る",
    "選択",
    "オプション",
    "ドライブ",
    "終了",

    "ファイル名",
    "デバイス",
    "サイズ",
    "更新日",
    "アーカイブ済み",
    "デバイスを選択",

    "詳細",
    "確認",
    "キャンセル",

    "ファイル",
    "設定",
    "このアプリについて",

    // Image/Text Viewer Hints
    "前へ",
    "次へ",
    "拡大",
    "縮小",
    "プロパティ",
    "全画面",
    "ZRを押して全画面を終了",

    // Reset Settings
    "設定をリセット",
    "すべての設定をデフォルトにリセットしますか？",
    "デフォルトに戻す",

    // Replace Confirmation
    "ファイルが既に存在します",
    "同じ名前のファイルが既に存在します。置き換えますか？",
    "置き換え",

    // Multi-file Replace Confirmation
    "%zu / %zu 件のファイルが既に存在します。",
    "すべて置き換え",
    "既存をスキップ",

    // Hex Mode
    "Hex",
    "ファイルを開く",
    "このファイルにはバイナリデータが含まれている可能性があります。どのモードで開きますか？",
    "テキストとして開く",
    "16進数で開く",

    // Button Style
    "ボタンスタイル",
    " カラー",
    " モノクロ",
    " アクセント",

    // Restart button
    "再起動",

    // Error messages
    "ファイルが空です (0 KB)",

    // NSP File Installation
    "NSPをインストール",
    "このNSPファイルをインストールしますか？\n\n警告: NSPをインストールするとBANされる可能性があります。",
    "NSPのインストールに成功しました。",
    "NSPのインストールに失敗しました。",
    "アプレットモードではNSPインストールは利用できません。",

    // NRO Forwarder NSP Creation
    "Create Forwarder NSP",
    "Create a forwarder NSP for this NRO?\n\nThis will generate an installable NSP that launches this homebrew.",
    "Forwarder NSP created successfully.",
    "Failed to create forwarder NSP.",
    "Keys file not found at sdmc:/switch/prod.keys",
    "Building forwarder NSP...",
    "Create Forwarder",

    // Install button
    "インストール",

    // Install popup
    "警告: ホーム画面へのインストールは\nBANの原因となる可能性があります。",
    "今後表示しない",

    // Success/Error Toasts
    "削除が完了しました。",
    "アーカイブの展開が完了しました。",
    "アーカイブの展開に失敗しました。"
};

static const char *strings_en[] {
    "OK",
    "Cancel",

    "Options",
    "Select All",
    "Clear All",
    "Properties",
    "Rename",
    "New Folder",
    "New File",
    "Copy",
    "Cannot copy a folder into itself.",
    "Move",
    "Paste",
    "Delete",
    "Set Archive Bit",
    "Enter name",
    "Enter folder name",
    "Enter file name",
    "Copying:",

    "Name: ",
    "Size: ",
    "Created: ",
    "Modified: ",
    "Accessed: ",
    "Width: ",
    "Height: ",

    "This action cannot be undone.",
    "Do you wish to delete the following:",
    "Do you wish to delete ",

    "Extract archive",
    "This action may take a while.",
    "Do you wish to extract ",
    "Extracting:",

    "Settings",
    "Sort Settings",
    "Language",
    "USB",
    "Unmount USB devices",
    "Image Viewer",
    "Developer Options",
    "Display Resolution",
    " Auto",
    " 1080p",
    " 720p",
    "About",
    "Update",
    "3rd Party Components",
    "Check for Updates",
    " Display filename",
    " Enter images in fullscreen",
    " Enable logs",
    "version",
    "Author",
    "Banner",
    "License",

    "Stats for nerds",
    " Show performance overlay",

    // Stats overlay strings
    "Resolution: %dx%d",
    "FPS: %.1f (%.2fms)",
    "CPU: %u MHz",
    "GPU: %u MHz",
    "RAM: %.1f/%.1f MB",
    "SOC Temp: %.1f°C",
    "Skin Temp: %.1f°C",
    "N/A",

    "Accent Color",
    "Reset",

    "Theme",
    "Auto",
    "Dark",
    "Light",

    "Update",
    "Could not connect to network.",
    "An update is available.",
    "Do you wish to download and install NX-Shell version ",
    "Update was successful.",
    "Please restart the application.",
    "You are on the latest version.",

    "Do you wish to unmount all the connected USB devices?",
    "The USB device can now be safely removed.",

    "The name cannot be empty.",

    "Open",
    "Back",
    "Select",
    "Options",
    "Drive",
    "Quit",

    "Filename",
    "Device",
    "Size",
    "Modified",
    "Archived",
    "Select Device",

    "Details",
    "Confirm",
    "Cancel",

    "Files",
    "Settings",
    "About",

    // Image/Text Viewer Hints
    "Prev",
    "Next",
    "Zoom In",
    "Zoom Out",
    "Properties",
    "Fullscreen",
    "Press ZR to exit fullscreen",

    // Reset Settings
    "Reset Settings",
    "This will reset all settings to their defaults. Are you sure?",
    "Reset Defaults",

    // Replace Confirmation
    "File Already Exists",
    "A file with this name already exists. Do you want to replace it?",
    "Replace",

    // Multi-file Replace Confirmation
    "%zu of %zu files already exist at the destination.",
    "Replace All",
    "Skip Existing",

    // Hex Mode
    "Hex",
    "Open File",
    "This file may contain binary data. How would you like to open it?",
    "Open as Text",
    "Open as Hex",

    // Button Style
    "Button Style",
    " Colored",
    " Mono",
    " Accent",

    // Restart button
    "Restart",

    // Error messages
    "File is empty (0 KB)",

    // NSP File Installation
    "Install NSP",
    "Do you want to install this NSP file?\n\nWarning: Installing NSPs may result in a ban.",
    "NSP installed successfully.",
    "Failed to install NSP.",
    "NSP installation not available in applet mode.",

    // NRO Forwarder NSP Creation
    "Create Forwarder NSP",
    "Create a forwarder NSP for this NRO?\n\nThis will generate an installable NSP that launches this homebrew.",
    "Forwarder NSP created successfully.",
    "Failed to create forwarder NSP.",
    "Keys file not found at sdmc:/switch/prod.keys",
    "Building forwarder NSP...",
    "Create Forwarder",

    // Install button
    "Install",

    // Install popup
    "Warning: Installing to the homescreen\nmay result in a ban.",
    "Don't show again",

    // Success/Error Toasts
    "Delete completed successfully.",
    "Archive extracted successfully.",
    "Failed to extract archive."
};

// French (need to check and verify)
static const char *strings_fr[] {
    "OK",
    "Annuler",

    "Options",
    "Tout sélectionner",
    "Tout effacer",
    "Propriétés",
    "Renommer",
    "Nouveau dossier",
    "Nouveau fichier",
    "Copier",
    "Impossible de copier un dossier dans lui-même.",
    "Déplacer",
    "Coller",
    "Supprimer",
    "Définir le bit d'archive",
    "Entrez le nom",
    "Entrez le nom du dossier",
    "Entrez le nom du fichier",
    "Copie en cours:",

    "Nom: ",
    "Taille: ",
    "Créé: ",
    "Modifié: ",
    "Accédé: ",
    "Largeur: ",
    "Hauteur: ",

    "Cette action est irréversible.",
    "Voulez-vous supprimer les éléments suivants:",
    "Voulez-vous supprimer ",

    "Extraire l'archive",
    "Cette action peut prendre un certain temps.",
    "Voulez-vous extraire ",
    "Extraction en cours:",

    "Paramètres",
    "Paramètres de tri",
    "Langue",
    "USB",
    "Démonter les périphériques USB",
    "Visionneuse d'images",
    "Options développeur",
    "Résolution d'affichage",
    " Auto",
    " 1080p",
    " 720p",
    "À propos",
    "Mise à jour",
    "Composants tiers",
    "Vérifier les mises à jour",
    " Afficher le nom du fichier",
    " Ouvrir les images en plein écran",
    " Activer les journaux",
    "version",
    "Auteur",
    "Bannière",
    "Licence",

    "Statistiques pour les nerds",
    " Afficher la superposition de performance",

    // Stats overlay strings
    "Résolution : %dx%d",
    "IPS : %.1f (%.2fms)",
    "CPU : %u MHz",
    "GPU : %u MHz",
    "RAM : %.1f/%.1f Mo (%.0f%%)",
    "Temp. SOC : %.1f°C",
    "Temp. boîtier : %.1f°C",
    "N/A",

    "Couleur d'accentuation",
    "Réinitialiser",

    "Thème",
    "Auto",
    "Sombre",
    "Clair",

    "Mise à jour",
    "Impossible de se connecter au réseau.",
    "Une mise à jour est disponible.",
    "Voulez-vous télécharger et installer NX-Shell version ",
    "Mise à jour réussie.",
    "Veuillez redémarrer l'application.",
    "Vous êtes sur la dernière version.",

    "Voulez-vous démonter tous les périphériques USB connectés?",
    "Le périphérique USB peut maintenant être retiré en toute sécurité.",

    "Le nom ne peut pas être vide.",

    "Ouvrir",
    "Retour",
    "Sélectionner",
    "Options",
    "Lecteur",
    "Quitter",

    "Nom de fichier",
    "Périphérique",
    "Taille",
    "Modifié",
    "Archivé",
    "Sélectionner le périphérique",

    "Détails",
    "Confirmer",
    "Annuler",

    "Fichiers",
    "Paramètres",
    "À propos",

    // Image/Text Viewer Hints
    "Préc.",
    "Suiv.",
    "Zoom +",
    "Zoom -",
    "Propriétés",
    "Plein écran",
    "Appuyez sur ZR pour quitter le plein écran",

    // Reset Settings
    "Réinitialiser les paramètres",
    "Cela réinitialisera tous les paramètres par défaut. Êtes-vous sûr ?",
    "Restaurer les valeurs par défaut",

    // Replace Confirmation
    "Le fichier existe déjà",
    "Un fichier avec ce nom existe déjà. Voulez-vous le remplacer ?",
    "Remplacer",

    // Multi-file Replace Confirmation
    "%zu sur %zu fichiers existent déjà à la destination.",
    "Tout remplacer",
    "Ignorer existants",

    // Hex Mode
    "Hex",
    "Ouvrir le fichier",
    "Ce fichier peut contenir des données binaires. Comment voulez-vous l'ouvrir ?",
    "Ouvrir comme texte",
    "Ouvrir comme hex",

    // Button Style
    "Style des boutons",
    " Couleur",
    " Mono",
    " Accent",

    // Restart button
    "Redémarrer",

    // Error messages
    "Le fichier est vide (0 Ko)",

    // NSP File Installation
    "Installer NSP",
    "Voulez-vous installer ce fichier NSP ?\n\nAttention : L'installation de NSP peut entraîner un bannissement.",
    "NSP installé avec succès.",
    "Échec de l'installation du NSP.",
    "L'installation NSP n'est pas disponible en mode applet.",

    // NRO Forwarder NSP Creation
    "Créer un NSP de redirection",
    "Créer un NSP de redirection pour ce NRO ?\n\nCela générera un NSP installable qui lance ce homebrew.",
    "NSP de redirection créé avec succès.",
    "Échec de la création du NSP de redirection.",
    "Fichier de clés introuvable à sdmc:/switch/prod.keys",
    "Création du NSP de redirection...",
    "Créer redirection",

    // Install button
    "Installer",

    // Install popup
    "Attention: L'installation sur l'écran\nd'accueil peut entraîner un bannissement.",
    "Ne plus afficher",

    // Success/Error Toasts
    "Suppression terminée avec succès.",
    "Archive extraite avec succès.",
    "Échec de l'extraction de l'archive."
};

static const char *strings_de[] {
    "OK",
    "Abbrechen",

    "Optionen",
    "Alle auswählen",
    "Alle entfernen",
    "Eigenschaften",
    "Umbenennen",
    "Neuer Ordner",
    "Neue Datei",
    "Kopieren",
    "Ein Ordner kann nicht in sich selbst kopiert werden.",
    "Verschieben",
    "Einfügen",
    "Löschen",
    "\"Archive Bit\" setzen",
    "Name eingeben",
    "Ordnername eingeben",
    "Dateiname eingeben",
    "Kopiere:",

    "Name: ",
    "Größe: ",
    "Erstellt: ",
    "Geändert: ",
    "Zugegriffen: ",
    "Breite: ",
    "Höhe: ",

    "Dies kann nicht rückgängig gemacht werden.",
    "Möchten Sie Folgendes löschen:",
    "Möchten Sie Folgendes löschen:",

    "Archiv entpacken",
    "Dies kann eine Weile dauern.",
    "Möchten Sie Folgendes extrahieren:",
    "Extrahiere:",

    "Einstellungen",
    "Sortiereinstellung",
    "Sprache",
    "USB",
    "USB-Geräte auswerfen",
    "Bildanzeige",
    "Entwickleroptionen",
    "Anzeigeauflösung",
    " Automatisch",
    " 1080p",
    " 720p",
    "Über",
    "Update",
    "Drittanbieter-Komponenten",
    "Nach Updates suchen",
    " Dateiname anzeigen",
    " Bilder im Vollbildmodus öffnen",
    " Log aktivieren",
    "Version",
    "Autor",
    "Banner",
    "Lizenz",

    "Statistik für Nerds",
    " Leistungsanzeige einblenden",

    // Stats overlay strings
    "Auflösung: %dx%d",
    "FPS: %.1f (%.2fms)",
    "CPU: %u MHz",
    "GPU: %u MHz",
    "RAM: %.1f/%.1f MB",
    "SOC-Temp.: %.1f°C",
    "Gehäuse-Temp.: %.1f°C",
    "N/A",

    "Akzentfarbe",
    "Zurücksetzen",

    "Thema",
    "Auto",
    "Dunkel",
    "Hell",

    "Update",
    "Es konnte keine Verbindung zum Netzwerk herstellt werden.",
    "Ein Update ist verfügbar.",
    "Möchten Sie die folgende NX-Shell-Version herunterladen und installieren:",
    "Update war erfolgreich.",
    "Bitte starten Sie die Anwendung neu.",
    "Sie sind bereits auf der neusten Version.",

    "Möchten Sie alle verbundenen USB-Geräte auswerfen?",
    "Das USB-Gerät kann jetzt sicher entfernt werden.",

    "Der Name darf nicht leer sein.",

    "Öffnen",
    "Zurück",
    "Auswählen",
    "Optionen",
    "Laufwerk",
    "Beenden",

    "Dateiname",
    "Gerät",
    "Größe",
    "Geändert",
    "Archiviert",
    "Gerät auswählen",

    "Details",
    "Bestätigen",
    "Abbrechen",

    "Dateien",
    "Einstellungen",
    "Über",

    // Image/Text Viewer Hints
    "Zurück",
    "Weiter",
    "Vergrößern",
    "Verkleinern",
    "Eigenschaften",
    "Vollbild",
    "ZR drücken um Vollbild zu beenden",

    // Reset Settings
    "Einstellungen zurücksetzen",
    "Alle Einstellungen werden auf die Standardwerte zurückgesetzt. Sind Sie sicher?",
    "Standardwerte wiederherstellen",

    // Replace Confirmation
    "Datei existiert bereits",
    "Eine Datei mit diesem Namen existiert bereits. Möchten Sie sie ersetzen?",
    "Ersetzen",

    // Multi-file Replace Confirmation
    "%zu von %zu Dateien existieren bereits am Zielort.",
    "Alle ersetzen",
    "Vorhandene überspringen",

    // Hex Mode
    "Hex",
    "Datei öffnen",
    "Diese Datei enthält möglicherweise Binärdaten. Wie möchten Sie sie öffnen?",
    "Als Text öffnen",
    "Als Hex öffnen",

    // Button Style
    "Tastenstil",
    " Farbig",
    " Mono",
    " Akzent",

    // Restart button
    "Neustart",

    // Error messages
    "Datei ist leer (0 KB)",

    // NSP File Installation
    "NSP installieren",
    "Möchten Sie diese NSP-Datei installieren?\n\nWarnung: Die Installation von NSPs kann zu einem Bann führen.",
    "NSP erfolgreich installiert.",
    "NSP-Installation fehlgeschlagen.",
    "NSP-Installation im Applet-Modus nicht verfügbar.",

    // NRO Forwarder NSP Creation
    "Forwarder-NSP erstellen",
    "Forwarder-NSP für dieses NRO erstellen?\n\nDies generiert ein installierbares NSP, das diese Homebrew startet.",
    "Forwarder-NSP erfolgreich erstellt.",
    "Forwarder-NSP konnte nicht erstellt werden.",
    "Schlüsseldatei nicht gefunden unter sdmc:/switch/prod.keys",
    "Erstelle Forwarder-NSP...",
    "Forwarder erstellen",

    // Install button
    "Installieren",

    // Install popup
    "Warnung: Die Installation auf dem Homescreen\nkann zu einem Bann führen.",
    "Nicht mehr anzeigen",

    // Success/Error Toasts
    "Löschen erfolgreich abgeschlossen.",
    "Archiv erfolgreich extrahiert.",
    "Archiv konnte nicht extrahiert werden."
};

// Italian (need to check and verify)
static const char *strings_it[] {
    "OK",
    "Annulla",

    "Opzioni",
    "Seleziona tutto",
    "Cancella tutto",
    "Proprietà",
    "Rinomina",
    "Nuova cartella",
    "Nuovo file",
    "Copia",
    "Impossibile copiare una cartella dentro sé stessa.",
    "Sposta",
    "Incolla",
    "Elimina",
    "Imposta bit di archiviazione",
    "Inserisci nome",
    "Inserisci nome cartella",
    "Inserisci nome file",
    "Copia in corso:",

    "Nome: ",
    "Dimensione: ",
    "Creato: ",
    "Modificato: ",
    "Accesso: ",
    "Larghezza: ",
    "Altezza: ",

    "Questa azione non può essere annullata.",
    "Vuoi eliminare quanto segue:",
    "Vuoi eliminare ",

    "Estrai archivio",
    "Questa azione potrebbe richiedere del tempo.",
    "Vuoi estrarre ",
    "Estrazione in corso:",

    "Impostazioni",
    "Impostazioni di ordinamento",
    "Lingua",
    "USB",
    "Smonta dispositivi USB",
    "Visualizzatore immagini",
    "Opzioni sviluppatore",
    "Risoluzione display",
    " Auto",
    " 1080p",
    " 720p",
    "Informazioni",
    "Aggiornamento",
    "Componenti di terze parti",
    "Verifica aggiornamenti",
    " Mostra nome file",
    " Apri immagini a schermo intero",
    " Abilita log",
    "versione",
    "Autore",
    "Banner",
    "Licenza",

    "Statistiche per nerd",
    " Mostra overlay prestazioni",

    // Stats overlay strings
    "Risoluzione: %dx%d",
    "FPS: %.1f (%.2fms)",
    "CPU: %u MHz",
    "GPU: %u MHz",
    "RAM: %.1f/%.1f MB",
    "Temp. SOC: %.1f°C",
    "Temp. scocca: %.1f°C",
    "N/D",

    "Colore di accentuazione",
    "Ripristina",

    "Tema",
    "Auto",
    "Scuro",
    "Chiaro",

    "Aggiornamento",
    "Impossibile connettersi alla rete.",
    "È disponibile un aggiornamento.",
    "Vuoi scaricare e installare NX-Shell versione ",
    "Aggiornamento riuscito.",
    "Per favore riavvia l'applicazione.",
    "Stai utilizzando l'ultima versione.",

    "Vuoi smontare tutti i dispositivi USB collegati?",
    "Il dispositivo USB può ora essere rimosso in sicurezza.",

    "Il nome non può essere vuoto.",

    "Apri",
    "Indietro",
    "Seleziona",
    "Opzioni",
    "Unità",
    "Esci",

    "Nome file",
    "Dispositivo",
    "Dimensione",
    "Modificato",
    "Archiviato",
    "Seleziona dispositivo",

    "Dettagli",
    "Conferma",
    "Annulla",

    "File",
    "Impostazioni",
    "Informazioni",

    // Image/Text Viewer Hints
    "Prec.",
    "Succ.",
    "Zoom +",
    "Zoom -",
    "Proprietà",
    "Schermo intero",
    "Premi ZR per uscire dalla modalità schermo intero",

    // Reset Settings
    "Ripristina impostazioni",
    "Tutte le impostazioni verranno ripristinate ai valori predefiniti. Sei sicuro?",
    "Ripristina predefiniti",

    // Replace Confirmation
    "Il file esiste già",
    "Un file con questo nome esiste già. Vuoi sostituirlo?",
    "Sostituisci",

    // Multi-file Replace Confirmation
    "%zu di %zu file esistono già nella destinazione.",
    "Sostituisci tutto",
    "Salta esistenti",

    // Hex Mode
    "Hex",
    "Apri file",
    "Questo file potrebbe contenere dati binari. Come vuoi aprirlo?",
    "Apri come testo",
    "Apri come hex",

    // Button Style
    "Stile pulsanti",
    " Colorato",
    " Mono",
    " Accento",

    // Restart button
    "Riavvia",

    // Error messages
    "Il file è vuoto (0 KB)",

    // NSP File Installation
    "Installa NSP",
    "Vuoi installare questo file NSP?\n\nAttenzione: L'installazione di NSP potrebbe causare un ban.",
    "NSP installato con successo.",
    "Installazione NSP fallita.",
    "Installazione NSP non disponibile in modalità applet.",

    // NRO Forwarder NSP Creation
    "Crea NSP forwarder",
    "Creare un NSP forwarder per questo NRO?\n\nQuesto genererà un NSP installabile che avvia questo homebrew.",
    "NSP forwarder creato con successo.",
    "Impossibile creare NSP forwarder.",
    "File delle chiavi non trovato in sdmc:/switch/prod.keys",
    "Creazione NSP forwarder...",
    "Crea forwarder",

    // Install button
    "Installa",

    // Install popup
    "Attenzione: L'installazione nella schermata\nprincipale potrebbe causare un ban.",
    "Non mostrare più",

    // Success/Error Toasts
    "Eliminazione completata con successo.",
    "Archivio estratto con successo.",
    "Impossibile estrarre l'archivio."
};

//  Spanish
static const char *strings_es[] {
    "Aceptar",
    "Cancelar",

    "Opciones",
    "Seleccionar Todo",
    "Limpiar Todo",
    "Propiedades",
    "Renombrar",
    "Nueva Carpeta",
    "Nuevo Archivo",
    "Copiar",
    "No se puede copiar una carpeta dentro de sí misma.",
    "Mover",
    "Pegar",
    "Eliminar",
    "Establecer Bit de Archivo",
    "Ingresar Nombre",
    "Ingresar Nombre de Carpeta",
    "Ingresar Nombre de Archivo",
    "Copiando:",

    "Nombre: ",
    "Tamaño: ",
    "Creado: ",
    "Modificado: ",
    "Accedido: ",
    "Ancho: ",
    "Alto: ",

    "Esta acción no se puede deshacer.",
    "Deseas eliminar lo siguiente:",
    "Deseas eliminar ",

    "Extraer archivo",
    "Esta acción puede tomar un tiempo.",
    "Deseas extraer ",
    "Extrayendo:",

    "Ajustes",
    "Ajustes de organización",
    "Idioma",
    "USB",
    "Unmount USB devices",
    "Visualizador de Imagen",
    "Opciones de Desarrollador",
    "Resolución de Pantalla",
    " Automático",
    " 1080p",
    " 720p",
    "Acerca de",
    "Actualización",
    "Componentes de terceros",
    "Buscar Actualizaciones",
    " Mostrar nombre de archivo",
    " Abrir imágenes en pantalla completa",
    " Habilitar logs",
    "versión",
    "Autor",
    "Banner",
    "Licencia",

    "Estadísticas para nerds",
    " Mostrar superposición de rendimiento",

    // Stats overlay strings
    "Resolución: %dx%d",
    "FPS: %.1f (%.2fms)",
    "CPU: %u MHz",
    "GPU: %u MHz",
    "RAM: %.1f/%.1f MB",
    "Temp. SOC: %.1f°C",
    "Temp. carcasa: %.1f°C",
    "N/D",

    "Color de acento",
    "Restablecer",

    "Tema",
    "Auto",
    "Oscuro",
    "Claro",

    "Actualizar",
    "No se puede conectar a la red.",
    "Una actualización está disponible.",
    "Deseas descargar e instalar la versión de NX-Shell ",
    "Actualización exitosa.",
    "Por favor reinicia la aplicación.",
    "Estás en la última versión.",

    "¿Quieres desmontar todos los dispositivos USB conectados?",
    "El dispositivo USB ahora puede ser removido de forma segura.",

    "El nombre no puede estar vacío.",

    "Abrir",
    "Atrás",
    "Seleccionar",
    "Opciones",
    "Unidad",
    "Salir",

    "Nombre",
    "Dispositivo",
    "Tamaño",
    "Modificado",
    "Archivado",
    "Seleccionar Dispositivo",

    "Detalles",
    "Confirmar",
    "Cancelar",

    "Archivos",
    "Ajustes",
    "Acerca de",

    // Image/Text Viewer Hints
    "Ant.",
    "Sig.",
    "Zoom +",
    "Zoom -",
    "Propiedades",
    "Pantalla completa",
    "Pulsa ZR para salir de pantalla completa",

    // Reset Settings
    "Restablecer ajustes",
    "Esto restablecerá todos los ajustes a sus valores predeterminados. ¿Estás seguro?",
    "Restaurar valores predeterminados",

    // Replace Confirmation
    "El archivo ya existe",
    "Ya existe un archivo con este nombre. ¿Desea reemplazarlo?",
    "Reemplazar",

    // Multi-file Replace Confirmation
    "%zu de %zu archivos ya existen en el destino.",
    "Reemplazar todo",
    "Omitir existentes",

    // Hex Mode
    "Hex",
    "Abrir archivo",
    "Este archivo puede contener datos binarios. ¿Cómo desea abrirlo?",
    "Abrir como texto",
    "Abrir como hex",

    // Button Style
    "Estilo de botones",
    " Color",
    " Mono",
    " Acento",

    // Restart button
    "Reiniciar",

    // Error messages
    "El archivo está vacío (0 KB)",

    // NSP File Installation
    "Instalar NSP",
    "¿Deseas instalar este archivo NSP?\n\nAdvertencia: Instalar NSPs puede resultar en un baneo.",
    "NSP instalado exitosamente.",
    "Error al instalar el NSP.",
    "Instalación NSP no disponible en modo applet.",

    // NRO Forwarder NSP Creation
    "Crear NSP forwarder",
    "¿Crear un NSP forwarder para este NRO?\n\nEsto generará un NSP instalable que lanza este homebrew.",
    "NSP forwarder creado exitosamente.",
    "Error al crear NSP forwarder.",
    "Archivo de claves no encontrado en sdmc:/switch/prod.keys",
    "Creando NSP forwarder...",
    "Crear forwarder",

    // Install button
    "Instalar",

    // Install popup
    "Advertencia: Instalar en la pantalla\nde inicio puede resultar en un baneo.",
    "No mostrar de nuevo",

    // Success/Error Toasts
    "Eliminación completada con éxito.",
    "Archivo extraído con éxito.",
    "Error al extraer el archivo."
};

// Simplified Chinese ("Chinese")
static const char *strings_sc[] {
    "确定",
    "取消",

    "选项",
    "选择全部",
    "清除全部",
    "属性",
    "重命名",
    "新建文件夹",
    "新建文件",
    "复制",
    "无法将文件夹复制到其自身中。",
    "移动",
    "粘贴",
    "删除",
    "设置存档位",
    "输入名字",
    "输入文件夹名",
    "输入文件名",
    "复制: ",

    "文件名: ",
    "大小: ",
    "创建日期: ",
    "最后修改: ",
    "最后访问: ",
    "宽度: ",
    "高度: ",

    "本操作不可逆.",
    "确定删除下列文件吗:",
    "确定删除吗 ",

    "提取归档",
    "本功能需要花费一点时间.",
    "确定提取吗 ",
    "提取中:",

    "设置",
    "排序方式",
    "语言",
    "USB",
    "卸载USB设备",
    "图片查看器",
    "开发人员选项",
    "显示分辨率",
    " 自动",
    " 1080p",
    " 720p",
    "关于",
    "更新",
    "第三方组件",
    "检查更新",
    " 显示文件名",
    " 以全屏模式打开图片",
    " 打开日志",
    "版本",
    "作者",
    "横幅",
    "许可证",

    "极客统计",
    " 显示性能叠加层",

    // Stats overlay strings
    "分辨率: %dx%d",
    "帧率: %.1f (%.2fms)",
    "CPU: %u MHz",
    "GPU: %u MHz",
    "RAM: %.1f/%.1f MB",
    "SOC温度: %.1f°C",
    "外壳温度: %.1f°C",
    "不可用",

    "主题色",
    "重置",

    "主题",
    "自动",
    "深色",
    "浅色",

    "更新",
    "连接网络失败.",
    "有新版本的更新可用.",
    "您希望下载并安装NX-Shell版本吗 ",
    "更新成功.",
    "请重新启动应用程序。",
    "你使用的是最新版本.",

    "您想卸载所有连接的 USB 设备吗？",
    "现在可以安全地移除 USB 设备。",

    "名称不能为空.",

    "打开",
    "返回",
    "选择",
    "选项",
    "驱动器",
    "退出",

    "文件名",
    "设备",
    "大小",
    "修改日期",
    "已归档",
    "选择设备",

    "详细信息",
    "确认",
    "取消",

    "文件",
    "设置",
    "关于",

    // Image/Text Viewer Hints
    "上一张",
    "下一张",
    "放大",
    "缩小",
    "属性",
    "全屏",
    "按ZR退出全屏",

    // Reset Settings
    "重置设置",
    "这将把所有设置重置为默认值。确定吗？",
    "恢复默认设置",

    // Replace Confirmation
    "文件已存在",
    "同名文件已存在。是否要替换？",
    "替换",

    // Multi-file Replace Confirmation
    "%zu / %zu 个文件在目标位置已存在。",
    "全部替换",
    "跳过已存在",

    // Hex Mode
    "十六进制",
    "打开文件",
    "此文件可能包含二进制数据。您想如何打开它？",
    "作为文本打开",
    "作为十六进制打开",

    // Button Style
    "按钮样式",
    " 彩色",
    " 单色",
    " 主题色",

    // Restart button
    "重启",

    // Error messages
    "文件为空 (0 KB)",

    // NSP File Installation
    "安装NSP",
    "您要安装此NSP文件吗？\n\n警告：安装NSP可能导致被封禁。",
    "NSP安装成功。",
    "NSP安装失败。",
    "小程序模式下无法安装NSP。",

    // NRO Forwarder NSP Creation
    "创建转发NSP",
    "为此NRO创建转发NSP？\n\n这将生成一个可安装的NSP来启动此自制软件。",
    "转发NSP创建成功。",
    "转发NSP创建失败。",
    "密钥文件未找到：sdmc:/switch/prod.keys",
    "正在创建转发NSP...",
    "创建转发",

    // Install button
    "安装",

    // Install popup
    "警告：安装到主屏幕可能\n导致被封禁。",
    "不再显示",

    // Success/Error Toasts
    "删除成功完成。",
    "归档提取成功。",
    "归档提取失败。"
};

// Korean (need to check and verify)
static const char *strings_ko[] {
    "확인",
    "취소",

    "옵션",
    "모두 선택",
    "모두 지움",
    "속성",
    "이름 바꾸기",
    "새 폴더",
    "새 파일",
    "복사",
    "폴더를 자기 자신 안에 복사할 수 없습니다.",
    "이동",
    "붙여넣기",
    "삭제",
    "아카이브 비트 설정",
    "이름 입력",
    "폴더 이름 입력",
    "파일 이름 입력",
    "복사 중:",

    "이름: ",
    "크기: ",
    "생성일: ",
    "수정일: ",
    "접근일: ",
    "너비: ",
    "높이: ",

    "이 작업은 취소할 수 없습니다.",
    "다음을 삭제하시겠습니까:",
    "삭제하시겠습니까 ",

    "아카이브 추출",
    "이 작업은 시간이 걸릴 수 있습니다.",
    "추출하시겠습니까 ",
    "추출 중:",

    "설정",
    "정렬 설정",
    "언어",
    "USB",
    "USB 장치 마운트 해제",
    "이미지 뷰어",
    "개발자 옵션",
    "디스플레이 해상도",
    " 자동",
    " 1080p",
    " 720p",
    "정보",
    "업데이트",
    "서드파티 구성 요소",
    "업데이트 확인",
    " 파일 이름 표시",
    " 전체 화면으로 이미지 열기",
    " 로그 활성화",
    "버전",
    "제작자",
    "배너",
    "라이선스",

    "고급 통계",
    " 성능 오버레이 표시",

    // Stats overlay strings
    "해상도: %dx%d",
    "FPS: %.1f (%.2fms)",
    "CPU: %u MHz",
    "GPU: %u MHz",
    "메모리: %.1f/%.1f MB (%.0f%%)",
    "SOC 온도: %.1f°C",
    "외부 온도: %.1f°C",
    "N/A",

    "강조 색상",
    "초기화",

    "테마",
    "자동",
    "다크",
    "라이트",

    "업데이트",
    "네트워크에 연결할 수 없습니다.",
    "업데이트가 가능합니다.",
    "NX-Shell 버전을 다운로드하여 설치하시겠습니까 ",
    "업데이트에 성공했습니다.",
    "애플리케이션을 다시 시작해 주세요.",
    "최신 버전을 사용 중입니다.",

    "연결된 모든 USB 장치를 마운트 해제하시겠습니까?",
    "이제 USB 장치를 안전하게 제거할 수 있습니다.",

    "이름은 비워둘 수 없습니다.",

    "열기",
    "뒤로",
    "선택",
    "옵션",
    "드라이브",
    "종료",

    "파일명",
    "장치",
    "크기",
    "수정됨",
    "보관됨",
    "장치 선택",

    "상세정보",
    "확인",
    "취소",

    "파일",
    "설정",
    "정보",

    // Image/Text Viewer Hints
    "이전",
    "다음",
    "확대",
    "축소",
    "속성",
    "전체화면",
    "ZR을 눌러 전체화면 종료",

    // Reset Settings
    "설정 초기화",
    "모든 설정이 기본값으로 초기화됩니다. 계속하시겠습니까?",
    "기본값으로 재설정",

    // Replace Confirmation
    "파일이 이미 존재합니다",
    "같은 이름의 파일이 이미 존재합니다. 교체하시겠습니까?",
    "교체",

    // Multi-file Replace Confirmation
    "%zu / %zu 개의 파일이 대상 위치에 이미 존재합니다.",
    "모두 교체",
    "기존 파일 건너뛰기",

    // Hex Mode
    "Hex",
    "파일 열기",
    "이 파일에는 바이너리 데이터가 포함될 수 있습니다. 어떻게 열겠습니까?",
    "텍스트로 열기",
    "16진수로 열기",

    // Button Style
    "버튼 스타일",
    " 컬러",
    " 모노",
    " 강조색",

    // Restart button
    "다시 시작",

    // Error messages
    "파일이 비어 있습니다 (0 KB)",

    // NSP File Installation
    "NSP 설치",
    "이 NSP 파일을 설치하시겠습니까?\n\n경고: NSP 설치는 차단될 수 있습니다.",
    "NSP가 성공적으로 설치되었습니다.",
    "NSP 설치에 실패했습니다.",
    "애플릿 모드에서는 NSP 설치를 사용할 수 없습니다.",

    // NRO Forwarder NSP Creation
    "포워더 NSP 생성",
    "이 NRO에 대한 포워더 NSP를 생성하시겠습니까?\n\n이 홈브류를 실행하는 설치 가능한 NSP가 생성됩니다.",
    "포워더 NSP가 성공적으로 생성되었습니다.",
    "포워더 NSP 생성에 실패했습니다.",
    "키 파일을 찾을 수 없습니다: sdmc:/switch/prod.keys",
    "포워더 NSP 생성 중...",
    "포워더 생성",

    // Install button
    "설치",

    // Install popup
    "경고: 홈 화면에 설치하면\n밴을 받을 수 있습니다.",
    "다시 표시 안 함",

    // Success/Error Toasts
    "삭제가 성공적으로 완료되었습니다.",
    "아카이브 추출 성공.",
    "아카이브 추출 실패."
};

// Dutch (need to check and verify)
static const char *strings_nl[] {
    "OK",
    "Annuleren",

    "Opties",
    "Alles selecteren",
    "Alles wissen",
    "Eigenschappen",
    "Hernoemen",
    "Nieuwe map",
    "Nieuw bestand",
    "Kopiëren",
    "Kan een map niet naar zichzelf kopiëren.",
    "Verplaatsen",
    "Plakken",
    "Verwijderen",
    "Archiefbit instellen",
    "Voer naam in",
    "Voer mapnaam in",
    "Voer bestandsnaam in",
    "Kopiëren:",

    "Naam: ",
    "Grootte: ",
    "Aangemaakt: ",
    "Gewijzigd: ",
    "Geopend: ",
    "Breedte: ",
    "Hoogte: ",

    "Deze actie kan niet ongedaan worden gemaakt.",
    "Wilt u het volgende verwijderen:",
    "Wilt u verwijderen ",

    "Archief uitpakken",
    "Deze actie kan even duren.",
    "Wilt u uitpakken ",
    "Uitpakken:",

    "Instellingen",
    "Sorteerinstellingen",
    "Taal",
    "USB",
    "USB-apparaten ontkoppelen",
    "Afbeeldingsviewer",
    "Ontwikkelaarsopties",
    "Schermresolutie",
    " Automatisch",
    " 1080p",
    " 720p",
    "Over",
    "Update",
    "Componenten van derden",
    "Controleren op updates",
    " Bestandsnaam weergeven",
    " Afbeeldingen openen in volledig scherm",
    " Logboeken inschakelen",
    "versie",
    "Auteur",
    "Banner",
    "Licentie",

    "Statistieken voor nerds",
    " Prestatie-overlay weergeven",

    // Stats overlay strings
    "Resolutie: %dx%d",
    "FPS: %.1f (%.2fms)",
    "CPU: %u MHz",
    "GPU: %u MHz",
    "Geheugen: %.1f/%.1f MB",
    "SOC-temp.: %.1f°C",
    "Behuizing-temp.: %.1f°C",
    "N.v.t.",

    "Accentkleur",
    "Herstellen",

    "Thema",
    "Auto",
    "Donker",
    "Licht",

    "Update",
    "Kan geen verbinding maken met het netwerk.",
    "Er is een update beschikbaar.",
    "Wilt u NX-Shell versie downloaden en installeren ",
    "Update is geslaagd.",
    "Herstart de applicatie alstublieft.",
    "U gebruikt de nieuwste versie.",

    "Wilt u alle aangesloten USB-apparaten ontkoppelen?",
    "Het USB-apparaat kan nu veilig worden verwijderd.",

    "De naam mag niet leeg zijn.",

    "Openen",
    "Terug",
    "Selecteren",
    "Opties",
    "Station",
    "Afsluiten",

    "Bestandsnaam",
    "Apparaat",
    "Grootte",
    "Gewijzigd",
    "Gearchiveerd",
    "Apparaat selecteren",

    "Details",
    "Bevestigen",
    "Annuleren",

    "Bestanden",
    "Instellingen",
    "Over",

    // Image/Text Viewer Hints
    "Vorige",
    "Volgende",
    "Zoom +",
    "Zoom -",
    "Eigenschappen",
    "Volledig scherm",
    "Druk op ZR om volledig scherm te verlaten",

    // Reset Settings
    "Instellingen herstellen",
    "Alle instellingen worden teruggezet naar de standaardwaarden. Weet u het zeker?",
    "Standaardwaarden herstellen",

    // Replace Confirmation
    "Bestand bestaat al",
    "Er bestaat al een bestand met deze naam. Wilt u het vervangen?",
    "Vervangen",

    // Multi-file Replace Confirmation
    "%zu van %zu bestanden bestaan al op de bestemming.",
    "Alles vervangen",
    "Bestaande overslaan",

    // Hex Mode
    "Hex",
    "Bestand openen",
    "Dit bestand kan binaire gegevens bevatten. Hoe wilt u het openen?",
    "Openen als tekst",
    "Openen als hex",

    // Button Style
    "Knopstijl",
    " Gekleurd",
    " Mono",
    " Accent",

    // Restart button
    "Herstarten",

    // Error messages
    "Bestand is leeg (0 KB)",

    // NSP File Installation
    "NSP installeren",
    "Wilt u dit NSP-bestand installeren?\n\nWaarschuwing: Het installeren van NSPs kan leiden tot een ban.",
    "NSP succesvol geïnstalleerd.",
    "NSP-installatie mislukt.",
    "NSP-installatie niet beschikbaar in applet-modus.",

    // NRO Forwarder NSP Creation
    "Forwarder NSP maken",
    "Forwarder NSP maken voor deze NRO?\n\nDit genereert een installeerbare NSP die deze homebrew start.",
    "Forwarder NSP succesvol gemaakt.",
    "Forwarder NSP maken mislukt.",
    "Sleutelbestand niet gevonden op sdmc:/switch/prod.keys",
    "Forwarder NSP maken...",
    "Forwarder maken",

    // Install button
    "Installeren",

    // Install popup
    "Waarschuwing: Installeren op het\nstartscherm kan leiden tot een ban.",
    "Niet meer tonen",

    // Success/Error Toasts
    "Verwijderen succesvol voltooid.",
    "Archief succesvol uitgepakt.",
    "Archief uitpakken mislukt."
};

// Portuguese
static const char *strings_pt[] {
    "OK",
    "Cancelar",

    "Opções",
    "Selecionar Tudo",
    "Limpar Tudo",
    "Propriedades",
    "Renomear",
    "Nova Pasta",
    "Novo Arquivo",
    "Copiar",
    "Não é possível copiar uma pasta para dentro dela mesma.",
    "Mover",
    "Colar",
    "Deletar",
    "Definir Bit de Arquivo",
    "Insira o nome",
    "Insira o nome da pasta",
    "Insira o nome do arquivo",
    "Copiando:",

    "Nome: ",
    "Tamanho: ",
    "Criado: ",
    "Modificado: ",
    "Acessado: ",
    "Largura: ",
    "Altura: ",

    "Essa ação não pode ser desfeita.",
    "Você deseja deletar os seguintes:",
    "Você deseja deletar ",

    "Extrair arquivo",
    "Essa ação pode demorar um pouco.",
    "Você deseja extrair ",
    "Extraindo:",

    "Configurações",
    "Configurações de Organização",
    "Idioma",
    "USB",
    "Desmontar dispositivos USB",
    "Visualizador de Imagens",
    "Opções de Desenvolvedor",
    "Resolução de Tela",
    " Automático",
    " 1080p",
    " 720p",
    "Sobre",
    "Atualização",
    "Componentes de terceiros",
    "Verificar se há Atualizações",
    " Exibir nome de arquivo",
    " Abrir imagens em tela cheia",
    " Habilitar logs",
    "versão",
    "Autor",
    "Banner",
    "Licença",

    "Estatísticas para nerds",
    " Mostrar sobreposição de desempenho",

    // Stats overlay strings
    "Resolução: %dx%d",
    "FPS: %.1f (%.2fms)",
    "CPU: %u MHz",
    "GPU: %u MHz",
    "RAM: %.1f/%.1f MB",
    "Temp. SOC: %.1f°C",
    "Temp. externa: %.1f°C",
    "N/D",

    "Cor de destaque",
    "Redefinir",

    "Tema",
    "Auto",
    "Escuro",
    "Claro",

    "Atualizar",
    "Não foi possível se conectar à internet.",
    "Uma atualização está disponível.",
    "Você deseja baixar e instalar NX-Shell versão ",
    "Atualização feita com sucesso.",
    "Por favor reinicie a aplicação.",
    "Você está na versão mais recente.",

    "Você deseja desmontar todos os dispositivos USB conectados?",
    "O dispositivo USB pode ser removido com segurança.",

    "O nome não pode estar vazio.",

    "Abrir",
    "Voltar",
    "Selecionar",
    "Opções",
    "Unidade",
    "Sair",

    "Nome do arquivo",
    "Dispositivo",
    "Tamanho",
    "Modificado",
    "Arquivado",
    "Selecionar Dispositivo",

    "Detalhes",
    "Confirmar",
    "Cancelar",

    "Arquivos",
    "Configurações",
    "Sobre",

    // Image/Text Viewer Hints
    "Ant.",
    "Próx.",
    "Zoom +",
    "Zoom -",
    "Propriedades",
    "Tela cheia",
    "Pressione ZR para sair da tela cheia",

    // Reset Settings
    "Redefinir configurações",
    "Isso redefinirá todas as configurações para os padrões. Tem certeza?",
    "Restaurar padrões",

    // Replace Confirmation
    "O arquivo já existe",
    "Um arquivo com este nome já existe. Deseja substituí-lo?",
    "Substituir",

    // Multi-file Replace Confirmation
    "%zu de %zu arquivos já existem no destino.",
    "Substituir tudo",
    "Ignorar existentes",

    // Hex Mode
    "Hex",
    "Abrir arquivo",
    "Este arquivo pode conter dados binários. Como você gostaria de abri-lo?",
    "Abrir como texto",
    "Abrir como hex",

    // Button Style
    "Estilo dos botões",
    " Colorido",
    " Mono",
    " Destaque",

    // Restart button
    "Reiniciar",

    // Error messages
    "O arquivo está vazio (0 KB)",

    // NSP File Installation
    "Instalar NSP",
    "Deseja instalar este arquivo NSP?\n\nAviso: Instalar NSPs pode resultar em banimento.",
    "NSP instalado com sucesso.",
    "Falha ao instalar o NSP.",
    "Instalação NSP não disponível no modo applet.",

    // NRO Forwarder NSP Creation
    "Criar NSP Forwarder",
    "Criar um NSP forwarder para este NRO?\n\nIsso gerará um NSP instalável que inicia este homebrew.",
    "NSP forwarder criado com sucesso.",
    "Falha ao criar NSP forwarder.",
    "Arquivo de chaves não encontrado em sdmc:/switch/prod.keys",
    "Criando NSP forwarder...",
    "Criar forwarder",

    // Install button
    "Instalar",

    // Install popup
    "Aviso: Instalar na tela inicial\npode resultar em banimento.",
    "Não mostrar novamente",

    // Success/Error Toasts
    "Exclusão concluída com sucesso.",
    "Arquivo extraído com sucesso.",
    "Falha ao extrair o arquivo."
};

// Russian (need to check and verify)
static const char *strings_ru[] {
    "ОК",
    "Отмена",

    "Параметры",
    "Выбрать все",
    "Очистить все",
    "Свойства",
    "Переименовать",
    "Новая папка",
    "Новый файл",
    "Копировать",
    "Невозможно скопировать папку в саму себя.",
    "Переместить",
    "Вставить",
    "Удалить",
    "Установить архивный бит",
    "Введите имя",
    "Введите имя папки",
    "Введите имя файла",
    "Копирование:",

    "Имя: ",
    "Размер: ",
    "Создан: ",
    "Изменён: ",
    "Доступ: ",
    "Ширина: ",
    "Высота: ",

    "Это действие нельзя отменить.",
    "Вы хотите удалить следующее:",
    "Вы хотите удалить ",

    "Извлечь архив",
    "Это действие может занять некоторое время.",
    "Вы хотите извлечь ",
    "Извлечение:",

    "Настройки",
    "Настройки сортировки",
    "Язык",
    "USB",
    "Отключить USB-устройства",
    "Просмотр изображений",
    "Параметры разработчика",
    "Разрешение экрана",
    " Авто",
    " 1080p",
    " 720p",
    "О программе",
    "Обновление",
    "Сторонние компоненты",
    "Проверить обновления",
    " Показывать имя файла",
    " Открывать изображения в полноэкранном режиме",
    " Включить журналы",
    "версия",
    "Автор",
    "Баннер",
    "Лицензия",

    "Статистика для гиков",
    " Показывать оверлей производительности",

    // Stats overlay strings
    "Разрешение: %dx%d",
    "FPS: %.1f (%.2fms)",
    "CPU: %u МГц",
    "GPU: %u МГц",
    "RAM: %.1f/%.1f МБ (%.0f%%)",
    "Темп. SOC: %.1f°C",
    "Темп. корпуса: %.1f°C",
    "Н/Д",

    "Цвет акцента",
    "Сбросить",

    "Тема",
    "Авто",
    "Тёмная",
    "Светлая",

    "Обновление",
    "Не удалось подключиться к сети.",
    "Доступно обновление.",
    "Вы хотите загрузить и установить NX-Shell версии ",
    "Обновление выполнено успешно.",
    "Пожалуйста, перезапустите приложение.",
    "У вас установлена последняя версия.",

    "Вы хотите отключить все подключённые USB-устройства?",
    "USB-устройство теперь можно безопасно извлечь.",

    "Имя не может быть пустым.",

    "Открыть",
    "Назад",
    "Выбрать",
    "Параметры",
    "Диск",
    "Выход",

    "Имя файла",
    "Устройство",
    "Размер",
    "Изменён",
    "Архивирован",
    "Выбрать устройство",

    "Подробности",
    "Подтвердить",
    "Отмена",

    "Файлы",
    "Настройки",
    "О программе",

    // Image/Text Viewer Hints
    "Пред.",
    "След.",
    "Увеличить",
    "Уменьшить",
    "Свойства",
    "Полный экран",
    "Нажмите ZR для выхода из полноэкранного режима",

    // Reset Settings
    "Сбросить настройки",
    "Все настройки будут сброшены до значений по умолчанию. Вы уверены?",
    "Восстановить по умолчанию",

    // Replace Confirmation
    "Файл уже существует",
    "Файл с таким именем уже существует. Заменить его?",
    "Заменить",

    // Multi-file Replace Confirmation
    "%zu из %zu файлов уже существуют в месте назначения.",
    "Заменить все",
    "Пропустить существующие",

    // Hex Mode
    "Hex",
    "Открыть файл",
    "Этот файл может содержать двоичные данные. Как вы хотите его открыть?",
    "Открыть как текст",
    "Открыть как hex",

    // Button Style
    "Стиль кнопок",
    " Цветной",
    " Моно",
    " Акцент",

    // Restart button
    "Перезапуск",

    // Error messages
    "Файл пустой (0 КБ)",

    // NSP File Installation
    "Установить NSP",
    "Вы хотите установить этот NSP файл?\n\nПредупреждение: Установка NSP может привести к бану.",
    "NSP успешно установлен.",
    "Не удалось установить NSP.",
    "Установка NSP недоступна в режиме апплета.",

    // NRO Forwarder NSP Creation
    "Создать NSP-форвардер",
    "Создать NSP-форвардер для этого NRO?\n\nБудет создан устанавливаемый NSP для запуска этого хомбрю.",
    "NSP-форвардер успешно создан.",
    "Не удалось создать NSP-форвардер.",
    "Файл ключей не найден: sdmc:/switch/prod.keys",
    "Создание NSP-форвардера...",
    "Создать форвардер",

    // Install button
    "Установить",

    // Install popup
    "Внимание: Установка на главный экран\nможет привести к бану.",
    "Больше не показывать",

    // Success/Error Toasts
    "Удаление успешно завершено.",
    "Архив успешно извлечён.",
    "Не удалось извлечь архив."
};

// Traditional Chinese ("Taiwanese")
static const char *strings_tw[] {
    "確定",
    "取消",

    "選項",
    "選擇全部",
    "清除全部",
    "屬性",
    "重命名",
    "新建文件夾",
    "新建文件",
    "復制",
    "無法將文件夾複製到其自身中。",
    "移動",
    "粘貼",
    "刪除",
    "設置存檔位",
    "輸入名字",
    "輸入文件夾名",
    "輸入文件名",
    "復制:",

    "文件名: ",
    "大小: ",
    "創建日期: ",
    "最後修改: ",
    "最後訪問: ",
    "寬度: ",
    "高度: ",

    "本操作不可逆.",
    "確定刪除下列文件嗎:",
    "確定刪除嗎 ",

    "提取歸檔",
    "本功能需要花費壹點時間.",
    "確定提取嗎 ",
    "提取中:",

    "設置",
    "排序方式",
    "語言",
    "USB",
    "卸載 USB 設備",
    "圖片查看器",
    "開發人員選項",
    "顯示解析度",
    " 自動",
    " 1080p",
    " 720p",
    "關於",
    "更新",
    "第三方組件",
    "檢查更新",
    " 顯示文件名",
    " 以全螢幕模式開啟圖片",
    " 打開日誌",
    "版本",
    "作者",
    "橫幅",
    "授權條款",

    "極客統計",
    " 顯示效能疊加層",

    // Stats overlay strings
    "解析度: %dx%d",
    "幀率: %.1f (%.2fms)",
    "CPU: %u MHz",
    "GPU: %u MHz",
    "RAM: %.1f/%.1f MB",
    "SOC溫度: %.1f°C",
    "外殼溫度: %.1f°C",
    "不適用",

    "主題色",
    "重置",

    "主題",
    "自動",
    "深色",
    "淺色",

    "更新",
    "連接網絡失敗.",
    "有新版本的更新可用.",
    "您希望下載並安裝NX-Shell版本嗎 ",
    "更新成功.",
    "請重新啟動應用程序。",
    "妳使用的是最新版本.",

    "您想卸載所有連接的 USB 設備嗎？",
    "現在可以安全地移除 USB 設備。",

    "名稱不能為空.",

    "打開",
    "返回",
    "選擇",
    "選項",
    "磁碟機",
    "退出",

    "檔案名稱",
    "裝置",
    "大小",
    "修改日期",
    "已歸檔",
    "選擇設備",

    "詳細資訊",
    "確認",
    "取消",

    "檔案",
    "設置",
    "關於",

    // Image/Text Viewer Hints
    "上一張",
    "下一張",
    "放大",
    "縮小",
    "屬性",
    "全螢幕",
    "按ZR退出全螢幕",

    // Reset Settings
    "重置設定",
    "這將把所有設定重置為預設值。確定嗎？",
    "恢復預設設定",

    // Replace Confirmation
    "檔案已存在",
    "同名檔案已存在。是否要取代？",
    "取代",

    // Multi-file Replace Confirmation
    "%zu / %zu 個檔案在目標位置已存在。",
    "全部取代",
    "跳過已存在",

    // Hex Mode
    "十六進制",
    "開啟檔案",
    "此檔案可能包含二進制資料。您想如何開啟它？",
    "作為文字開啟",
    "作為十六進制開啟",

    // Button Style
    "按鈕樣式",
    " 彩色",
    " 單色",
    " 主題色",

    // Restart button
    "重新啟動",

    // Error messages
    "檔案為空 (0 KB)",

    // NSP File Installation
    "安裝NSP",
    "您要安裝此NSP檔案嗎？\n\n警告：安裝NSP可能導致被封禁。",
    "NSP安裝成功。",
    "NSP安裝失敗。",
    "小程式模式下無法安裝NSP。",

    // NRO Forwarder NSP Creation
    "建立轉發NSP",
    "為此NRO建立轉發NSP？\n\n這將產生一個可安裝的NSP來啟動此自製軟體。",
    "轉發NSP建立成功。",
    "轉發NSP建立失敗。",
    "密鑰檔案未找到：sdmc:/switch/prod.keys",
    "正在建立轉發NSP...",
    "建立轉發",

    // Install button
    "安裝",

    // Install popup
    "警告：安裝到主畫面可能\n導致被封禁。",
    "不再顯示",

    // Success/Error Toasts
    "刪除成功完成。",
    "歸檔提取成功。",
    "歸檔提取失敗。"
};

const char **strings[Lang::Max] = {
    strings_jp,
    strings_en,
    strings_fr,
    strings_de,
    strings_it,
    strings_es,
    strings_sc,
    strings_ko,
    strings_nl,
    strings_pt,
    strings_ru,
    strings_tw
};
