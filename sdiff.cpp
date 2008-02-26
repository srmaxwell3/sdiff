#include <map>
using std::map;
#include <string>
using std::string;
#include <stdio.h>

int main (int argc, char *argv[])
{
  map<string, int> table;
  for (int a = 1; a < argc; a += 1) {
    if (FILE *f = fopen (argv[a], "r")) {
      fprintf (stderr, "# Reading %s...", argv[a]);
      char buffer[1024];
      unsigned n = 0;
      while (fgets (buffer, sizeof (buffer), f)) {
	string l (buffer);
	table[l] += 1;
	n += 1;
      }
      fprintf (stderr, " %u lines.\n", n);
    } else {
    }
  }
  printf ("table.size () = %d\n", table.size ());
  map<int, int> counts;
  for (map<string, int>::iterator i = table.begin (); i != table.end (); i++) {
    counts[i->second] += 1;
  }
  for (map<int, int>::iterator i = counts.begin (); i != counts.end (); i++) {
    fprintf (stderr, "counts[%d] = %d\n", i->first, i->second);
  }
}
