HSQL = /path/to/hyrise/sql-parser/
CFLAGS = -std=c++1z -lstdc++ -Wall -Werror -I$(HSQL)src/ -L$(HSQL) -Wl,-R$(HSQL)

all:
	g++ $(CFLAGS) HyriseExample.cpp -o hyrise_example -lsqlparser