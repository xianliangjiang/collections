How To run Skype from the command line
=====================================

Installing:
https://help.ubuntu.com/community/Skype

If on 64-bit architecture enable MultiArch:
`$ sudo dpkg --add-architecture i386`

then (for all systems)
`
$ sudo add-apt-repository "deb http://archive.canonical.com/ $(lsb_release -sc) partner"
$ sudo apt-get update && sudo apt-get install skype
`


`
$ /usr/local/bin/skype --callto:echo123 &
`

--callto: <Skype user / telephone number>
	

Places call to the specified user or number

--disable-api
	

Disables Skype's Application Programming Interface (API)

--use-session-bus
	

Uses session bus rather than default bus
