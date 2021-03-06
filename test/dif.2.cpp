#include <string>
using std::string;
#include <list>
using std::list;
#include <map>
using std::map;
#include <vector>
using std::vector;
#include <stdio.h>

struct Line : public string {
  Line (string _text) :
    text (_text)
  {
  }
  string text;
  list<unsigned> counts[2];
};

typedef map<string, Line *> MapStringToLinePtr;
typedef vector<Line *> VectorLinePtr;

int main (int argc, char *argv[])
{
  MapStringToLinePtr table;
  VectorLinePtr lines[2];

  for (int a = 1; a < argc && a <= 2; a += 1) {
    if (FILE *f = fopen (argv[a], "r")) {
      fprintf (stderr, "# Reading %s...", argv[a]);

      VectorLinePtr *lines = new VectorLinePtr ();
      char buffer[1024];
      while (fgets (buffer, sizeof (buffer), f)) {
        string text (buffer);

        Line *line = table[text];

        if (!line) {
          table[text] = line = new Line (text);
        }
        line->counts[a - 1].push_back (lines[a - 1].size ());
        lines[a - 1].push_back (line);
      }
      fprintf (stderr, " %u lines.\n", lines[a - 1]->size ());
    } else {
      fprintf (stderr, "%s: Unable to open %s!  Ignoring....\n", argv[a]);
    }
  }
  printf ("# table.size () = %d\n", table.size ());
  map<int, int> counts;
  for (map<string, int>::iterator i = table.begin (); i != table.end (); i++) {
    Line *l = i->second;
    counts[l->counts[0].size () + l->counts[1].size ()] += 1;
  }
  for (map<int, int>::iterator i = counts.begin (); i != counts.end (); i++) {
    fprintf (stderr, "# counts[%d] = %d\n", i->first, i->second);
  }
  for (int f = 0; f < 2; f += 1) {
    fprintf (stderr, "# file #%d\n", f + 1);
    for (int l = 0; l < lines[f].size (); l += 1) {
      fprintf (stderr, "#   [%d] = %d\n", l, lines[f][l]->counts[f].size ());
    }
  }
}
