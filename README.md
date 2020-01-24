# ctf_delivery
This is a framework for the easy creation and deployment of network-based CTF challenges. Its aim is to allow for the inclusion of
remote boxes to cryptography or reverse-engineering oriented challenges, thus giving challenge creators more options. The inspiration
for this project was its utility in challenge creation for the inaugural Calvert Hall Capture the Flag Competition.

# Use
This project constitutes a daemon (`deliveryd.c`) and its client controller (`delivery.c`), which can be used to start and monitor any networked ctf challenge
that can be executed by the linux kernel. Any individual challenge to be run is defined as a module - such modules should be documented in
the `modules` folder, using the file `example.mod` as a template. The challenge executables that correspond to the module files should be stored in the
`bin` folder, and should have names matching the names in their module files. See `bin/example.c` for an example of a functional module.

Any executable file can be used for a challenge module, including scripts that begin with a "shebang" invocation. Please refer to the client controller help text
for additional usage information.

# Author
Jason Walter (walterj21@chcstudent.com)

# License
This project is licensed with the GNU General Public License Version 3.

