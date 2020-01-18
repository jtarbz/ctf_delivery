# ctf_delivery
this is a framework for the easy creation and deployment of network-based ctf challenges. its aim is to allow for the inclusion of
remote boxes to cryptography or reverse-engineering oriented challenges, thus giving challenge creators more options. the inspiration
for this project was its utility in challenge creation for the inaugural calvert hall capture the flag competition.

# use
this project constitutes a daemon (`deliveryd.c`) and its client controller (`delivery.c`), which can be used to start and monitor any networked ctf challenge
that can be executed by the linux kernel. any individual challenge to be run is defined as a module - such modules should be documented in
the `modules` folder, using the file `example.mod` as a template. the challenge executables that correspond to the module files should be stored in the
`bin` folder, and should have names matching the names in their module files.

any executable file can be used for a challenge module, including scripts that begin with a "shebang" invocation. please refer to the client controller help text
for additional usage information.

I NEED TO MAKE THIS README WAY BETTER I KNOW
# author
**jason walter** - sole creator and god of this world

# license
this project is licensed with the gnu gplv3

