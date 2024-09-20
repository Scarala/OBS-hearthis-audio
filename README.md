# Hearthis.at Audio Stream Script for OBS Studio

This is an OBS Python script for streaming a PC audio source to a Hearthis.at audio stream (Icecast2).

German version below!

---

## Installation

### 1. Install Python
Download and install Python from the official website: [Python Downloads](https://www.python.org/downloads/).

### 2. Download the Script
You can download the script in 3 ways:
- **Clone the repository** (if you have Git installed):
  ```bash
  git clone https://github.com/Scarala/OBS-hearthis-audio
  ```
*   **Download the ZIP** of the repository:
    
    *   Click the "<> Code" button and select "Download ZIP".
        
*   **Download the raw script** directly:
    
    *   Click on hearthis-audio.py and then click "Download raw file" at the top right of the code window.
        

### 3\. Configure Python in OBS

Open OBS, navigate to Tools > Scripts > Python Settings, and click "Browse" to select your Python installation.

### 4\. Add the Script to OBS

Go to the Scripts tab, click the + icon, and add the script.If you downloaded the repository as a ZIP, make sure to unpack it first.

### 5\. Install FFmpeg

Download and unpack FFmpeg from this link:[FFmpeg Latest Build](https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl.zip)

### 6\. Configure FFmpeg in OBS

In OBS, select ffmpeg.exe by clicking search under FFmpeg-Path.

### 7\. Set Up Stream Data

Get your stream details from Hearthis.at. Fill out the fields in the script, select your audio source and bitrate, and click "Save Settings".

You're now ready to stream!

Streaming PC Audio instead of an Audio Source
---------------------------------------------

If you'd like to stream desktop audio instead of an input device (e.g., mic, USB interface), enable Stereo Mix in your PC audio settings.

### Enabling Stereo Mix in Windows 10

1.  **Open Sound Settings**

    Right-click the speaker icon in the system tray (bottom-right corner), and select "Sounds".
    
2.  **Show Disabled Devices**

    In the Sound window, go to the Recording tab, right-click, and select "Show Disabled Devices" and "Show Disconnected Devices".
    
3.  **Enable Stereo Mix**

    If "Stereo Mix" appears, right-click and select "Enable".
    
4.  **Adjust Stereo Mix Properties (Optional)**

    Right-click on "Stereo Mix" and choose "Properties" to adjust the volume under Levels or format settings under Advanced.
    
5.  **Update or Install Audio Drivers**

    If "Stereo Mix" is missing, update your drivers:
    
    *   Go to Device Manager, expand Sound, video, and game controllers, and update your audio driver.
        
    *   If needed, install Realtek drivers from your PC manufacturer or the [Realtek website](https://www.realtek.com).
        
6.  **Restart Your Computer**

    Restart to apply the changes.
    

### Enabling Stereo Mix in Windows 11

1.  **Open Sound Settings**

    Right-click the speaker icon in the system tray and select "Sound settings".
    
2.  **Open More Sound Settings**

    Scroll down and click "More sound settings".
    
3.  **Continue from Step 2 of the Windows 10 instructions**.
    

Hearthis.at Audio Stream Script für OBS Studio
==============================================

Dies ist ein OBS-Python-Skript, um einen PC-Audio-Eingang an einen Hearthis.at-Audiostream (Icecast2) zu streamen.

Installation
------------

### 1\. Installiere Python

Lade Python von der offiziellen Website herunter und installiere es: [Python Downloads](https://www.python.org/downloads/).

### 2\. Lade das Skript herunter

Du kannst das Skript auf 3 Arten herunterladen:

- **Klone das Repository** (wenn du Git installiert hast):
```bash
  git clone https://github.com/Scarala/OBS-hearthis-audio
  ```
    
*   **ZIP des Repositorys herunterladen**:
    
    *   Klicke auf den "<> Code"-Button und wähle "Download ZIP".
        
*   **Das Skript direkt herunterladen**:
    
    *   Klicke auf hearthis-audio.py und dann oben rechts auf "Download raw file".
        

### 3\. Python in OBS konfigurieren

Öffne OBS, navigiere zu Tools > Skripte > Python-Einstellungen und klicke auf "Browse", um die Python-Installation auszuwählen.

### 4\. Füge das Skript zu OBS hinzu

Gehe zum SKripte-Tab, klicke auf das + Symbol und füge das Skript hinzu. Falls du das Repository als ZIP heruntergeladen hast, entpacke es zuerst.

### 5\. Installiere FFmpeg

Lade und entpacke FFmpeg von diesem Link:[FFmpeg Letzte Version](https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl.zip)

### 6\. Konfiguriere FFmpeg in OBS

Wähle in OBS die ffmpeg.exe aus, indem du bei FFmpeg-Path auf druchsuchen klickst.

### 7\. Trage die Streaming-Daten ein

Hole dir deine Streamdaten von Hearthis.at, fülle die Felder aus, wähle die Audioquelle und Bitrate und klicke auf "Save Settings".

Du bist jetzt bereit zum Streamen!

PC-Audio anstatt einer Audioquelle streamen
-------------------------------------------

Wenn du anstatt eines Eingabegeräts (z. B. Mikrofon, USB-Interface) den Desktop-Audio streamen möchtest, musst du Stereo Mix in den Audioeinstellungen aktivieren.

### Stereo Mix in Windows 10 aktivieren

1.  **Soundeinstellungen öffnen**

    Klicke mit der rechten Maustaste auf das Lautsprechersymbol in der Taskleiste und wähle "Sounds".
    
2.  **Deaktivierte Geräte anzeigen**

    Gehe im Sound-Fenster zum Aufnahme-Tab, klicke mit der rechten Maustaste und wähle "Deaktivierte Geräte anzeigen" und "Getrennte Geräte anzeigen".
    
3.  **Stereo Mix aktivieren**

    Falls "Stereo Mix" erscheint, klicke mit der rechten Maustaste darauf und wähle "Aktivieren".
    
4.  **Stereo Mix Eigenschaften anpassen (Optional)**

    Klicke mit der rechten Maustaste auf "Stereo Mix" und wähle "Eigenschaften", um die Lautstärke unter Pegel oder die Format-Einstellungen unter Erweitert anzupassen.
    
5.  **Treiber aktualisieren oder installieren**

    Falls "Stereo Mix" fehlt, aktualisiere die Audiotreiber:
    
    *   Gehe in den Geräte-Manager, erweitere Audio-, Video- und Gamecontroller und aktualisiere den Audiotreiber.
        
    *   Wenn nötig, installiere die Realtek-Treiber von deinem PC-Hersteller oder der [Realtek-Website](https://www.realtek.com).
        
6.  **Computer neu starten**

    Starte den Computer neu, um die Änderungen anzuwenden.
    

### Stereo Mix in Windows 11 aktivieren

1.  **Soundeinstellungen öffnen**

    Klicke mit der rechten Maustaste auf das Lautsprechersymbol in der Taskleiste und wähle "Soundeinstellungen".
    
2.  **Weitere Soundeinstellungen öffnen**

    Scrolle nach unten und klicke auf "Weitere Soundeinstellungen".
    
3.  **Ab Schritt 2 von Windows 10 fortfahren**.
