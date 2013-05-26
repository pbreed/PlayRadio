PLATFORM=PK70
#This is a minimal make file.
#anything that starts with a # is a comment
#
#To generate the dependancies automatically 
#run "make depend"
#
#
#To clean up the directory 
#run "make clean"
#Your responsibilities as a programmer:
#
# Run make depend whenever:
#	You add files to the project
#	You change what files are included in a source file
#
# make clean whenever you change this makefile.
#
#Setup the project root name
#This will built NAME.s19 and save it as $(NBROOT)/bin/NAME.s19+
#This will built NAME_App.s19 and save it as $(NBROOT)/bin/NAME_App.s19+
NAME	= PlayRadio
CXXSRCS := main.cpp filesystemutils.cpp wav.cpp
#Uncomment and modify these lines if you have C or S files.
#CSRCS := foo.c
#ASRCS := foo.s
#CREATEDTARGS := htmldata.cpp


#include the file that does all of the automagic work!
include $(NBROOT)/make/main.mak


htmldata.cpp : $(wildcard html/*.*)
	comphtml html -ohtmldata.cpp




