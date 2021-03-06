#include <string>
using std::string;
#include <map>
using std::map;
#include <vector>
using std::vector;
#include <stdio.h>

int main (int argc, char *argv[])
{
  map<string, int> table;
  vector<vector<int *> *> fileLines;

  for (int a = 1; a < argc; a += 1) {
    if (FILE *f = fopen (argv[a], "r")) {
      fprintf (stderr, "# Reading %s...", argv[a]);
      vector<int *> *lines = new vector<int *> ();

      char buffer[1024];
      while (fgets (buffer, sizeof (buffer), f)) {
        int *line = &table[buffer];
        lines->push_back (line);
        *line += 1;
      }
      fprintf (stderr, " %u lines.\n", lines->size ());
      fileLines.push_back (lines);
    } else {
      fprintf (stderr, "%s: Unable to open %s!  Ignoring....\n", argv[a]);
    }
  }
  printf ("# table.size () = %d\n", table.size ());
  map<int, int> counts;
  for (map<string, int>::iterator i = table.begin (); i != table.end (); i++) {
    counts[i->second] += 1;
  }
  for (map<int, int>::iterator i = counts.begin (); i != counts.end (); i++) {
    fprintf (stderr, "# counts[%d] = %d\n", i->first, i->second);
  }
  for (int f = 0; f < fileLines.size (); f += 1) {
    fprintf (stderr, "# file #%d\n", f);
    vector<int *> *lines = fileLines[f];
    for (int l = 0; l < lines->size (); l += 1) {
      fprintf (stderr, "#   [%d] = %d\n", l, *(*lines)[l]);
    }
  }
}
