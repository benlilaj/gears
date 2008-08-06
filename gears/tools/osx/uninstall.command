#!/bin/sh

echo "Removing Gears:"

# Make sure script is run as root.
if [ $USER != 'root' ]; then
  sudo "$0" $*
  exit $?
fi

echo "removing InputManager"
rm -rf "/Library/InputManagers/GearsEnabler"
echo "removing Gears.plugin"
rm -rf "/Library/Internet Plug-Ins/Gears.plugin/"
echo "removing installer package receipt"
rm -rf "/Library/Receipts/Gears.pkg"

ksadmin=/Library/Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/Contents/MacOS/ksadmin
$ksadmin --delete --productid "{0006FF50-C0C0-4B9B-973C-4CF98BF4EA9D}"

echo "Gears for Safari was uninstalled succesfully."
