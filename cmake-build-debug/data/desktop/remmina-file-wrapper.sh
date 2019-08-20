#!/bin/bash -
#===============================================================================
#
#          FILE: remmina-file-wrapper.sh
#
#         USAGE: ./remmina-file-wrapper.sh
#
#   DESCRIPTION: Wrapper used by xdg to connect or edit a remmina file clicking
#                on it, or clicking to an URL like remmina:///profile.remmina
#
#       OPTIONS: File path or URL
#  REQUIREMENTS: ---
#          BUGS: ---
#         NOTES: ---
#        AUTHOR: Antenore Gatta (tmow), antenore@simbiosi.org
#  ORGANIZATION: Remmina
#       CREATED: 15. 06. 19 00:32:11
#      REVISION:  ---
#       LICENSE: GPLv2
#===============================================================================

set -o nounset                                  # Treat unset variables as an error

case "$@" in
	*rdp:*)
		/usr/local/bin/remmina "${@#rdp:\/\/}"
		;;
	*spice:*)
		/usr/local/bin/remmina "${@#spice:\/\/}"
		;;
	*vnc:*)
		/usr/local/bin/remmina "${@#vnc:\/\/}"
		;;
	*remmina:*)
		/usr/local/bin/remmina "${@#remmina:\/\/}"
		;;
	*)
		/usr/local/bin/remmina "${@}"
		;;
esac
