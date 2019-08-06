#!/bin/bash

###################################################################
# Written By:
#             Christian Kissinger
#
###################################################################


# Deploy kisslight to specified server
KL_IP=192.168.7.123

# specify user
USER=pi

usage()
{
    echo -e "Usage: For simply deploying and running:
    $0 
"

    exit 1
}

if [[ "$1" =~ ^[Hh]|[Hh][Ee][Ll][Pp]|-[Hh]|--[Hh][Ee][Ll][Pp]$ ]]; then
    usage
fi

if [ -f ../kisslight ]; then
    rsync -av ../kisslight $USER@$KL_IP:/home/$USER/
    ssh $USER@$KL_IP sudo /home/$USER/kisslight
    ssh $USER@$KL_IP sudo killall kisslight
else
    echo "kisslight binary not found, is it built yet?"

    exit 1
fi

exit 0