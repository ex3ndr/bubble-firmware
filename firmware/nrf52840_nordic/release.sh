set -e

# Cleanup
rm -fr release

# Define the board
BOARD="xiao_ble"

# List all configuration files
CONFIGS=(
    "bubble_pcm" 
    "bubble_mulaw" 
    "bubble_opus"
    "friend_pcm"
    "friend_mulaw"
    "friend_opus"
)

# Loop through each configuration and build the firmware
for CONFIG in "${CONFIGS[@]}"
do
    echo "Building with configuration $CONFIG"
    # Run the build command
    west build --pristine -b $BOARD -d release/$CONFIG -- -DCONF_FILE="prj.conf prj_$CONFIG.conf"
done