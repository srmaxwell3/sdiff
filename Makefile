OPT = -g

all:		sdiff

sdiff:		sdiff.o
	g++ $(OPT) -o $@ $<

sdiff.o:	sdiff.cpp
	g++ $(OPT) -c $<

clean:
	$(RM) sdiff.o

clobber:	clean
	$(RM) sdiff
