# Anastasia2
Anastasia: Apache module for scripting large and complex XML (and SGML) documents iwth TCL

On all systems: Anastasia is configured to work with Apache 2 (2.4.41 as of May 2020). You need to install Apache 2 with the apxs option so that you can use apxs to compile the Anastasia code into an Apache module (a .so, shared object, file in unix systems). This configuration assumes that you have installed Apache in /usr/local/apache2.

# On Linux/unix systems

a. Get the source for tcl. 8.4.13. cd into the unix folder, and configure as follows:
./configure  --enable-threads --disable-shared --disable-corefoundation

This will build a makefile. Then make. This should put libtcl8.4.a in your unix folder. Move this into the same folder as the Anastasia source.

b. Go into the anastasia folder containing the Anastasai source. Run this apxs:

apxs -i -c mod_anastasia.c ana_browsegrove.c ana_commands.c ana_memory.c ana_utils.c sgrep/common.c sgrep/eval.c sgrep/index.c sgrep/main.c sgrep/optimize.c  sgrep/parser.c sgrep/pmatch.c sgrep/sgml.c sgrep/sysdeps.c libtcl8.4.a -lm

NOTE: thanks to the guy on stack overflow ctx who pointed out we need to include libm -- which is what -lm points at. Same guy suggested ": You could also try to link the shared version of the tcl library instead by replacing libtcl8.4.a by -ltcl8.4 (if the corresponding tcl library is installed correctly in the system). Linking static libraries to a shared library can be problematic and should be avoided." I have not done that.

This should make the anastasia.so shared object file and put it in the modules folder in your Apache installation. (you might have to move this manually).

c. You then need to activate and invoke the Anastasia module from your server. Go to your httpd.conf file. Insert these lines:

LoadModule anastasia_module modules/mod_anastasia.so

and 

<Location /AnaServer>
		SetHandler anastasia-handler	
		Allow from all
		 Require all granted
	</Location>
  
  All should work! good luck.
  
  # On MacOS
  
  As a Unix based system -- it would be nice if exactly the same configuration as for unix worked. It is possible to use a complicated Mac only configuration, using gcc instead of apxs. For example: the tcl library is linked in a separate step as follows:
  
  gcc -DSHARED_MODULE -bundle -undefined suppress  -flat_namespace   -o mod_anastasia.so  Release/*.o libtcl8.5.a 
  
  We hope never to go down that route again.
  
  # On Windows etc
  
  Good luck. If you do it and it works please doucment.

