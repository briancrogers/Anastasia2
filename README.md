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
  
  Good luck. If you do it and it works please document.
  
  # MySQL support
  
  Version 1 of Anastasia had some neat mySQL support built in. The idea was to hook Anastasia direct to a mySQL database primarily for access control, and also to use databases for some complex data retrieval where XML is not so smart. However, we ended up going in another direction for access control (essentially -- using a Django set up for user management) and we stopped maintaining the mySQL functions. The code is still embedded into the files however: you are welcome to resurrect it and good luck.  However, some publications did use MySQL however. For these, the mysqltcl library is now used, as follows:
  
  a. We installed the libmysqltcl3.052.so after compilation in /usr/lib/tcltk/x86_64-linux-gnu/mysqltcl-3.052/
  
  b. We included this line at the start of each .tcl script file using a mySQL database:
  
        load "/usr/lib/tcltk/x86_64-linux-gnu/mysqltcl-3.052/libmysqltcl3.052.so"
 
  c. We connect to the database in that scripting file so:
  
  	set m [mysqlconnect -user root -db mysql -password MYPASSWORD]
	mysqluse $m MYDATABASE
	
  We then use calls as set out in the mysqltcl documentation. For example:
  
  	set dbResults [mysqlsel $m $sql -list]
	
  uses the mysqltcl command mysqlsel to run a SQL query in $sql on our database $m and returns the results as a tcl list. Very nifty.


