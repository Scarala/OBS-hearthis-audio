import obspython as obs
import subprocess
import threading
import re
import os
import signal
import webbrowser

#############################################################################
# hearthis-audio.py - Version 1.0.0                                         #
# Github: https://github.com/Scarala/OBS-hearthis-audio                     #
# Author: Scarala (https://scarala.de)                                      #
# OBS Studio Python Script for streaming audio only stream to Hearthis.at   #
#############################################################################

# Icecast Server Details (default values)
ICECAST_HOST = "streamlive2.hearthis.at"
ICECAST_PORT = 8080
ICECAST_PASSWORD = ""
ICECAST_USER = ""

FFMPEG_PATH = ""
ffmpeg_process = None
streaming_active = False
stream_thread = None

# Metadata (default values)
ICECAST_NAME = "My Stream Name"
ICECAST_DESCRIPTION = "A cool stream description"
ICECAST_GENRE = "My Genre"

# Audio devices
audio_devices = []

# Global settings variable
global_settings = None
status = ""

# Track if settings have been changed but not saved
settings_changed = False

def script_description():
    return "<h2>Hearthis.at Audio Only Stream Script</h2>Audio only Stream Script (icecast2) for Streaming a PC Audio Source to Hearthis.at<br><font color='#cc0000'>Script is for Windows systems Only!</font><br>Open Scriptlog for Script infos and Streamstatus.<br>Detailed instructions on Github: https://github.com/Scarala/OBS-hearthis-audio<br><br>Dependencies:<br>- Python 3.10 or higher<br>- ffmpeg.exe Version 7.x.x or higher."""

def script_properties():
    props = obs.obs_properties_create()
        
    # FFmpeg Path
    obs.obs_properties_add_button(props,"ffmpeg_link","Download latest ffmpeg", ffmpeg)
    obs.obs_properties_add_path(props, "ffmpeg_path", "FFmpeg Path", obs.OBS_PATH_FILE, "ffmpeg.exe (ffmpeg.exe)", None)

    # Icecast Server Settings
    obs.obs_properties_add_text(props, "icecast_info", "Find your Hearthis stream data at Hearthis:", obs.OBS_TEXT_INFO)
    obs.obs_properties_add_button(props,"ht_link","Open HearThis.at Stream page", ht)
    #obs.obs_property_button_set_url(ht_link, "https://hearthis.at/live/#audio-only")
    
    obs.obs_properties_add_text(props, "icecast_user", "Hearthis Stream User", obs.OBS_TEXT_DEFAULT)
    obs.obs_properties_add_text(props, "icecast_password", "Hearthis Stream Password", obs.OBS_TEXT_PASSWORD)

    # Metadata Settings
    obs.obs_properties_add_text(props, "ice_name", "Stream Name", obs.OBS_TEXT_DEFAULT)
    obs.obs_properties_add_text(props, "ice_description", "Stream Description", obs.OBS_TEXT_DEFAULT)
    obs.obs_properties_add_text(props, "ice_genre", "Stream Genre", obs.OBS_TEXT_DEFAULT)

    # Audio Source Dropdown
    update_audio_devices()
    audio_source_property = obs.obs_properties_add_list(props, "audio_source", "Audio Source", obs.OBS_COMBO_TYPE_EDITABLE, obs.OBS_COMBO_FORMAT_STRING)
    for device in audio_devices:
        obs.obs_property_list_add_string(audio_source_property, device, device)

    # Bitrate Dropdown
    bitrate_property = obs.obs_properties_add_list(props, "bitrate", "Bitrate", obs.OBS_COMBO_TYPE_EDITABLE, obs.OBS_COMBO_FORMAT_STRING)
    bitrates = ["32k", "64k", "128k", "192k", "256k", "320k"]
    for bitrate in bitrates:
        obs.obs_property_list_add_string(bitrate_property, bitrate, bitrate)

    # Checkbox Settings
    obs.obs_properties_add_bool(props, "onair_checkbox", "Auto OnAir")
    obs.obs_properties_add_bool(props, "recording_checkbox", "Auto Recording")
    
    # Add a Save Settings button
    obs.obs_properties_add_button(props, "save_settings_button", "Save Settings", save_settings_button)
    
    # Add buttons to start and stop streaming
    obs.obs_properties_add_button(props, "start_button", "Start/Stop Streaming", button)
    
    obs.obs_properties_add_text(props, "info", "Author: Scarala", obs.OBS_TEXT_INFO)

    return props
    
def ht(props, prop):
    weblink("https://hearthis.at/live/#audio-only")
        
def ffmpeg(props, prop):
    weblink("https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl.zip")
    
def weblink(link):
    webbrowser.open(link)

def script_defaults(settings):
    # Default Icecast connection settings
    obs.obs_data_set_default_string(settings, "icecast_user", ICECAST_USER)
    obs.obs_data_set_default_string(settings, "icecast_password", ICECAST_PASSWORD)

    # Default metadata settings
    obs.obs_data_set_default_string(settings, "ice_name", ICECAST_NAME)
    obs.obs_data_set_default_string(settings, "ice_description", ICECAST_DESCRIPTION)
    obs.obs_data_set_default_string(settings, "ice_genre", ICECAST_GENRE)
    
    obs.script_log(obs.LOG_INFO, "Hearthis.at Streaming Script initialized!")

def script_load(settings):
    """Wird aufgerufen, wenn das Skript geladen wird. Stellt sicher, dass die gespeicherten Einstellungen übernommen werden."""
    apply_settings(settings)

def script_update(settings):
    global global_settings
    global settings_changed

    global_settings = settings  # Speichert die Änderungen, aber wendet sie nicht an
    
    # Set settings_changed to True only if it was False before
    if settings_changed:
        obs.script_log(obs.LOG_INFO, "Settings changed but not applied. Please click 'Save Settings' to apply changes.")
    
    settings_changed = False
    
def save_settings_button(props, prop):
    save_settings()

def save_settings():
    """Wird aufgerufen, wenn der Benutzer auf 'Save Settings' klickt. Speichert und übernimmt die neuen Einstellungen."""
    global global_settings
    global settings_changed

    # Wendet die gespeicherten Einstellungen an
    apply_settings(global_settings)

    # Set settings_changed to False as changes have been saved
    settings_changed = False
    
    obs.script_log(obs.LOG_INFO, "Settings have been saved and applied.")

def apply_settings(settings):
    """Übernimmt die Einstellungen und speichert sie für den Stream."""
    global ICECAST_USER, ICECAST_PASSWORD, ICECAST_NAME, ICECAST_DESCRIPTION, ICECAST_GENRE, FFMPEG_PATH
    
    # Update Icecast connection settings
    ICECAST_USER = obs.obs_data_get_string(settings, "icecast_user")
    ICECAST_PASSWORD = obs.obs_data_get_string(settings, "icecast_password")

    # Update metadata settings
    ICECAST_NAME = obs.obs_data_get_string(settings, "ice_name")
    ICECAST_DESCRIPTION = obs.obs_data_get_string(settings, "ice_description")
    ICECAST_GENRE = obs.obs_data_get_string(settings, "ice_genre")

    # Update FFmpeg path
    FFMPEG_PATH = obs.obs_data_get_string(settings, "ffmpeg_path")

    # Update checkbox status
    onair_checked = obs.obs_data_get_bool(settings, "onair_checkbox")
    recording_checked = obs.obs_data_get_bool(settings, "recording_checkbox")

    # Update Stream Description with checkbox texts
    ICECAST_DESCRIPTION = "#OnAir " + ICECAST_DESCRIPTION if onair_checked else ICECAST_DESCRIPTION
    ICECAST_DESCRIPTION = "#Recording " + ICECAST_DESCRIPTION if recording_checked else ICECAST_DESCRIPTION

    obs.script_log(obs.LOG_INFO, "Settings have been applied.")

def update_audio_devices():
    global audio_devices
    try:
        # Run FFmpeg command to list audio devices
        command = ['ffmpeg', '-list_devices', 'true', '-f', 'dshow', '-i', 'dummy']
        result = subprocess.run(command, capture_output=True, text=True, shell=True, check=True)
        output = result.stderr
        
        # Extract audio device names using regex
        input_devices = re.findall(r'\[dshow @.*?\] "(.+?)" \((audio)\)', output)
        audio_devices = [device for device, type in input_devices]
    except subprocess.CalledProcessError as e:
        obs.script_log(obs.LOG_ERROR, f"Failed to list audio devices: {e}")

def validate_settings():
    ffmpeg_path = obs.obs_data_get_string(global_settings, "ffmpeg_path")
    bitrate = obs.obs_data_get_string(global_settings, "bitrate")
    audio_source = obs.obs_data_get_string(global_settings, "audio_source")

    errors = []
    
    # Überprüfe den FFmpeg-Pfad
    if not os.path.isfile(ffmpeg_path):
        errors.append("FFmpeg executable not found at specified path.")
    
    # Überprüfe die Bitrate
    if not bitrate:
        errors.append("Bitrate is not set.")
    
    # Überprüfe die Audioquelle
    if not audio_source:
        errors.append("Audio source is not selected.")
    
    if errors:
        for error in errors:
            obs.script_log(obs.LOG_ERROR, error)  # Gibt Fehlermeldungen in der OBS-Konsole aus
        return False
    
    return True

def get_ffmpeg_command():
    """ Constructs the FFmpeg command to stream to Icecast server with metadata """
    audio_source = obs.obs_data_get_string(global_settings, "audio_source")
    bitrate = obs.obs_data_get_string(global_settings, "bitrate")
    icecast_url = f"icecast://{ICECAST_USER}:{ICECAST_PASSWORD}@{ICECAST_HOST}:{ICECAST_PORT}/{ICECAST_USER}.ogg"

    command = [
        FFMPEG_PATH,
        "-f", "dshow",                    # Use dshow for Windows audio input
        "-i", f"audio={audio_source}",    # Input source (user-selected audio device)
        "-acodec", "libmp3lame",          # MP3 encoding
        "-b:a", bitrate,                  # Bitrate from user input
        "-content_type", "audio/mpeg",    # Set MIME type to audio/mpeg
        "-f", "mp3",                      # Format
        "-ice_name", ICECAST_NAME,        # Stream title from user input
        "-ice_description", ICECAST_DESCRIPTION, # Stream description from user input
        "-ice_genre", ICECAST_GENRE,      # Stream genre from user input
        icecast_url                       # Output URL to Icecast server
    ]

    return command

def start_ffmpeg_stream():
    global ffmpeg_process

    ffmpeg_command = get_ffmpeg_command()

    try:
        # Start FFmpeg process with stdin accessible
        ffmpeg_process = subprocess.Popen(ffmpeg_command, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        obs.script_log(obs.LOG_INFO, "FFmpeg started streaming.")
        
        # Read FFmpeg output in real-time
        for line in iter(ffmpeg_process.stderr.readline, ''):
            if "time" in line.strip():
                obs.script_log(obs.LOG_INFO, "Live-Time: " + line.strip().split('=')[2].split(' ')[0])  # Log FFmpeg stderr to OBS log

    except Exception as e:
        obs.script_log(obs.LOG_ERROR, f"Error starting FFmpeg: {e}")

def send_stop_signal():
    global ffmpeg_process

    if ffmpeg_process is not None:
        try:
            ffmpeg_process.stdin.write('q\n')  # Send "q" to FFmpeg
            ffmpeg_process.stdin.flush()  # Make sure the command is sent
            obs.script_log(obs.LOG_INFO, "Stop signal sent to FFmpeg.")
        except Exception as e:
            obs.script_log(obs.LOG_ERROR, f"Error sending stop signal to FFmpeg: {e}")

def stop_ffmpeg_stream():
    global ffmpeg_process
    
    if ffmpeg_process is not None:
        try:
            send_stop_signal()  # Send "q" to stop FFmpeg
            ffmpeg_process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            os.kill(ffmpeg_process.pid, signal.SIGKILL)  # Force the Process to exit, even if it is not terminated while timeout
            ffmpeg_process.wait()  # Wait until process exited
        finally:
            ffmpeg_process = None
            obs.script_log(obs.LOG_INFO, "FFmpeg streaming stopped.")

def stream_to_icecast():
    global streaming_active
    global ffmpeg_process
    streaming_active = True

    # Start FFmpeg in a separate thread to stream audio
    start_ffmpeg_stream()

    # Keep the main thread alive while streaming
    while streaming_active:
        try:
            # Here you can add any additional logic or monitoring if needed.
            pass
        except Exception as e:
            obs.script_log(obs.LOG_ERROR, f"Error during streaming: {e}")
            break
            
def button(props, prop):
    global stream_thread, streaming_active
    
    if not streaming_active:
        save_settings()
        start_streaming()
    else:
        stop_streaming()

def start_streaming():
    global stream_thread, streaming_active
    
    if not streaming_active:
        if not validate_settings():
            return  # Abbrechen, wenn die Validierung fehlschlägt
        obs.script_log(obs.LOG_INFO, "Hearthis.at Stream starting...")
        stream_thread = threading.Thread(target=stream_to_icecast)
        stream_thread.start()
        streaming_active = True

def stop_streaming():
    global streaming_active
    streaming_active = False
    obs.script_log(obs.LOG_INFO, "Hearthis.at Stream stopping...")
    stop_ffmpeg_stream()

    if stream_thread is not None:
        stream_thread.join()
    obs.script_log(obs.LOG_INFO, "Hearthis.at Stream ended!")
