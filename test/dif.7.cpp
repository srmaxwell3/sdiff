#include <algorithm>
using std::find;
#include <string>
using std::string;
#include <list>
using std::list;
#include <map>
using std::map;
#include <vector>
using std::vector;
#include <stdio.h>

enum DiffFormat {
  Default,

  Context,
  EdScript,
  IfThenElse,
  Normal,
  RCS,
  SideBySide,
  Unified
};

DiffFormat diffFormat = SideBySide;

unsigned optCopyContext = 3;
unsigned optPageWidth = 130;
unsigned optVerbose = 2;

struct Line : public string {
  Line (string _text) :
    string (_text)
  {
  }
  ~Line () {
    copies[0].erase (copies[0].begin (), copies[0].end ());
    copies[1].erase (copies[1].begin (), copies[1].end ());
  }
  void Dump (FILE *out) {
    fprintf (out, "{{ ");
    for (list<unsigned>::iterator i = copies[0].begin (); i != copies[0].end (); i++) {
      fprintf (out, "%u ", *i);
    }
    fprintf (out, "},{ ");
    for (list<unsigned>::iterator i = copies[1].begin (); i != copies[1].end (); i++) {
      fprintf (out, "%u ", *i);
    }
    fprintf (out, "}}\t%s\n", c_str ());
  }
  list<unsigned> copies[2];
};

struct LinePtr {
  LinePtr (Line *_line) :
    line (_line),
    l (~0u)
  {
  }
  void Dump (FILE *out) {
    fprintf (out, "[%d] ", l);
    line->Dump (out);
  }
  Line *line;
  unsigned l;
};

typedef map<string, Line *> MapStringToLinePtr;
typedef vector<LinePtr> VectorLinePtr;

// Our name.

char *ARGV0 = "dif3";

// The 'symbol' (line) table.

MapStringToLinePtr table;

// The names of the 2 files to be diff'd.

char const *files[2] = { 0, 0 };

// The lines of the 2 files to be diff'd.

VectorLinePtr lines[2];

// Miscellaneous metrics.

unsigned nTotalMatchedBlocks = 0;
unsigned nMatchedBlocks = 0;
unsigned nTotalMatchedLines = 0;
unsigned nMatchedLines = 0;

void pass1 ();
void pass2 ();
void pass3 ();
void pass4 ();
void pass5 ();
void pass6 ();

int main (int argc, char *argv[])
{
  // Read the old [0] file, and the new [1] file.

  for (int a = 1, n = 0; a < argc && n < 2; a += 1, n += 1) {
    files[n] = argv[a];

    if (FILE *f = fopen (files[n], "r")) {
      if (0 < optVerbose) {
        fprintf (stderr, "# Reading %s...", files[n]);
        fflush (stderr);
      }

      char buffer[1024];
      unsigned nUniq = 0;

      while (fgets (buffer, sizeof (buffer), f)) {
        string text (buffer);

        text.erase (text.end () - 1);

        Line *line = table[text];

        if (!line) {
          table[text] = line = new Line (text);
          nUniq += 1;
        }
        line->copies[n].push_back (lines[n].size ());
        lines[n].push_back (line);
      }

      if (0 < optVerbose) {
        fprintf (stderr, " %u lines, %u unique.\n", lines[n].size (), nUniq);
        fflush (stderr);
      }
    } else {
      fprintf (stderr, "%s: Unable to open %s!  Exiting....\n", ARGV0, files[n]);
      fflush (stderr);
      perror (ARGV0);
      exit (1);
    }
  }

  if (0 < optVerbose) {
    fprintf (stderr, "# Total unique lines = %d\n", table.size ());
    fflush (stderr);
  }

  // map<int, int> counts;
  // for (MapStringToLinePtr::iterator i = table.begin (); i != table.end (); i++) {
  //   Line *l = i->second;
  //   counts[l->counts[0].size () + l->counts[1].size ()] += 1;
  // }
  // for (map<int, int>::iterator i = counts.begin (); i != counts.end (); i++) {
  //   fprintf (stderr, "# counts[%d] = %d\n", i->first, i->second);
  // }

  pass1 ();
  pass2 ();
  pass3 ();
  pass4 ();
  pass5 ();
  pass6 ();

  return 0;
}

// Perform pass #1, in which we find all explicit matches amongst the
// unique lines.  For every unique line that appears the same number
// of times in both files, we declare the lines to match.

void pass1 ()
{
  if (0 < optVerbose) {
    fprintf (stderr, "# Pass 1 (finding matches amongst unique lines)...");
    fflush (stderr);
  }

  nMatchedLines = 0;

  for (MapStringToLinePtr::iterator i = table.begin (); i != table.end (); i++) {
    Line *l = i->second;

    // If the number of old and new lines match, then we declare that
    // they are (all) the same line.  Note, it isn't possible for the
    // numbers of both files to be 0.

    // (In the original algorithm, we only recognized a match, here,
    // if there was exactly 1 copy for each file.)

    if (l->copies[0].size () == l->copies[1].size ()) {
      nMatchedLines += 1;

      // For each matched line in the new file, mark it with the
      // corresponding line (number) in the old file.

      while (!l->copies[0].empty ()) {
        unsigned o = l->copies[0].front ();
        unsigned n = l->copies[1].front ();

        // Match up the pair.

        lines[0][o].l = n;
        lines[1][n].l = o;

        // Remove the matched line numbers from the lists.

        l->copies[0].pop_front ();
        l->copies[1].pop_front ();
      }
    }
  }
  nTotalMatchedLines += nMatchedLines;

  // Let's see the intermediate results.

  if (0 < optVerbose) {
    fprintf (stderr, " found %u matched lines (%u total).\n", nMatchedLines, nTotalMatchedLines);
    fflush (stderr);

    if (2 < optVerbose) {
      for (int f = 0; f < 2; f += 1) {
        fprintf (stderr, "# file #%d\n", f + 1);
        fflush (stderr);
        for (int l = 0; l < lines[f].size (); l += 1) {
          fprintf (stderr, "#   [%d] ", l);
          fflush (stderr);
          lines[f][l].Dump (stderr);
        }
      }
    }
  }
}

// Perform Pass #2, in which we attempt to 'widen' blocks of matched
// lines, by appending lines which are the same in both files.

void pass2 ()
{
  if (0 < optVerbose) {
    fprintf (stderr, "# Pass #2 (spreading matches down)...");
    fflush (stderr);
  }

  nMatchedBlocks = 0;
  nMatchedLines = 0;

  // Starting at the top of the old file,...

  for (unsigned o = 0; o < lines[0].size (); o += 1) {

    // ... skip any currently unmatched lines...

    if (lines[0][o].l == ~0) {
      continue;
    }

    // ... until we find an [already] matched line.  Then, skip over
    // the set of 1 or more matched lines, until we reach any
    // following, still-unmatched line.

    unsigned n;
    do {
      n = lines[0][o].l + 1;
      o += 1;
    } while (o < lines[0].size () && lines[0][o].l != ~0);

    // Now, o - 1 is the line number of the last matched old line, and
    // n - 1 is the line number of the last matched new line.  We want
    // to add new old and new lines, if they match.

    // Finally, attempt to add unmatched lines to the preceeding
    // matched set.

    for (; o < lines[0].size () && lines[0][o].l == ~0; o += 1, n += 1) {

      // If the [next] pair of old and new lines aren't the same line, we're done.

      if (lines[0][o].line != lines[1][n].line) {
        break;
      }

      // Match up the pair.

      lines[0][o].l = n;
      lines[1][n].l = o;

      // Remove the matched line numbers form the lists.

      Line *line = lines[0][o].line;
      list<unsigned> &oldCopies = line->copies[0];
      list<unsigned>::iterator oL = find (oldCopies.begin (), oldCopies.end (), o);
      if (oL != oldCopies.end ()) {
        oldCopies.erase (oL);
      }
      list<unsigned> &newCopies = line->copies[0];
      list<unsigned>::iterator nL = find (newCopies.begin (), newCopies.end (), n);
      if (nL != newCopies.end ()) {
        newCopies.erase (nL);
      }

      nMatchedLines += 1;
    }
    nMatchedBlocks += 1;
  }
  nTotalMatchedLines += nMatchedLines;
  nTotalMatchedBlocks += nMatchedBlocks;

  // Let's see the intermediate results.

  if (0 < optVerbose) {
    fprintf (stderr,
             " found %u/%u matched lines/blocks (%u/%u totals).\n",
             nMatchedLines,
             nMatchedBlocks,
             nTotalMatchedLines,
             nTotalMatchedBlocks
            );
    fflush (stderr);
    if (2 < optVerbose) {
      for (int f = 0; f < 2; f += 1) {
        fprintf (stderr, "# file #%d\n", f + 1);
        fflush (stderr);
        for (int l = 0; l < lines[f].size (); l += 1) {
          fprintf (stderr, "#   [%d] ", l);
          fflush (stderr);
          lines[f][l].Dump (stderr);
        }
      }
    }
  }
}

// Perform Pass #3, in which we attempt to 'widen' blocks of matched
// lines, by prepending lines which are the same in both files.

void pass3 ()
{
  if (0 < optVerbose) {
    fprintf (stderr, "# Pass #3 (spreading matches up)...");
    fflush (stderr);
  }

  nMatchedBlocks = 0;
  nMatchedLines = 0;

  // Starting at the bottom of the old file,...

  for (int o = lines[0].size (); 0 <= o; o -= 1) {

    // ... skip any currently unmatched lines...

    if (lines[0][o].l == ~0) {
      continue;
    }

    // ... until we find an [already] matched line.  Then, skip over
    // the set of 1 or more matched lines, until we reach any
    // following, still-unmatched line.

    unsigned n;
    do {
      n = lines[0][o].l - 1;
      o -= 1;
    } while (0 <= o && lines[0][o].l != ~0);

    // Now, o + 1 is the line number of the last matched old line, and
    // n + 1 is the line number of the last matched new line.  We want
    // to add new old and new lines, if they match.

    // Finally, attempt to add unmatched lines to the preceeding
    // matched set.

    for (; 0 <= o && lines[0][o].l == ~0; o -= 1, n -= 1) {

      // If the [next] pair of old and new lines aren't the same line, we're done.

      if (lines[0][o].line != lines[1][n].line) {
        break;
      }

      // Match up the pair.

      lines[0][o].l = n;
      lines[1][n].l = o;

      // Remove the matched line numbers form the lists.

      Line *line = lines[0][o].line;
      list<unsigned> &oldCopies = line->copies[0];
      list<unsigned>::iterator oL = find (oldCopies.begin (), oldCopies.end (), o);
      if (oL != oldCopies.end ()) {
        oldCopies.erase (oL);
      }
      list<unsigned> &newCopies = line->copies[0];
      list<unsigned>::iterator nL = find (newCopies.begin (), newCopies.end (), n);
      if (nL != newCopies.end ()) {
        newCopies.erase (nL);
      }

      nMatchedLines += 1;
    }
    nMatchedBlocks += 1;
  }
  nTotalMatchedLines += nMatchedLines;

  // Let's see the intermediate results.

  if (0 < optVerbose) {
    fprintf (stderr,
             " found %u/%u matched lines/blocks (%u/%u totals).\n",
             nMatchedLines,
             nMatchedBlocks,
             nTotalMatchedLines,
             nTotalMatchedBlocks
            );
    fflush (stderr);

    if (2 < optVerbose) {
      for (int f = 0; f < 2; f += 1) {
        fprintf (stderr, "# file #%d\n", f + 1);
        fflush (stderr);
        for (int l = 0; l < lines[f].size (); l += 1) {
          fprintf (stderr, "#   [%d] ", l);
          fflush (stderr);
          lines[f][l].Dump (stderr);
        }
      }
    }
  }
}

// Perform pass #4, in which we find any remaining matches amongst the
// unique lines.  For every unique line that has more than 1 copy in
// both files, we declare the copies to match.

void pass4 ()
{
  if (0 < optVerbose) {
    fprintf (stderr, "# Pass 4 (finding remaining matches amongst unique lines)...");
    fflush (stderr);
  }

  nMatchedLines = 0;

  for (MapStringToLinePtr::iterator i = table.begin (); i != table.end (); i++) {
    Line *l = i->second;

    // If there's more than 1 matching old and new lines, then we
    // declare that each pair are (all) the same line.

    // (In the original algorithm, we only recognized a match, here,
    // if there was exactly 1 copy for each file.)

    while (0 < l->copies[0].size () && 0 < l->copies[1].size ()) {
      nMatchedLines += 1;

      // For each matched line in the new file, mark it with the
      // corresponding line (number) in the old file.

      unsigned o = l->copies[0].front ();
      unsigned n = l->copies[1].front ();

      // Match up the pair.

      lines[0][o].l = n;
      lines[1][n].l = o;

      // Remove the matched line numbers from the lists.

      l->copies[0].pop_front ();
      l->copies[1].pop_front ();
    }
  }
  nTotalMatchedLines += nMatchedLines;

  // Let's see the intermediate results.

  if (0 < optVerbose) {
    fprintf (stderr, " found %u matched lines (%u total).\n", nMatchedLines, nTotalMatchedLines);
    fflush (stderr);

    if (2 < optVerbose) {
      for (int f = 0; f < 2; f += 1) {
        fprintf (stderr, "# file #%d\n", f + 1);
        fflush (stderr);
        for (int l = 0; l < lines[f].size (); l += 1) {
          fprintf (stderr, "#   [%d] ", l);
          fflush (stderr);
          lines[f][l].Dump (stderr);
        }
      }
    }
  }
}

// Perform Pass #5, in which we look for blocks which match, but are
// the result of a move (their not in their original position).  For
// these, we'll unmatch the blocks, turning them into a delete and an
// insert.

void pass5 ()
{
  if (0 < optVerbose) {
    fprintf (stderr, "# Pass #5 (unmatching block moves)...\n");
    fflush (stderr);
  }

  nMatchedBlocks = 0;
  nMatchedLines = 0;

  // Starting at the top of both files,...

  unsigned o = 0;
  unsigned n = 0;

  while (o < lines[0].size () || n < lines[1].size ()) {

    // Skip any unmatched lines at this point in the old file.  These
    // are deletes.

    //    Old     New
    //   +---+
    // 0 | a |
    //   +---+   +---+
    //         0 | b |
    //   +---+   +---+
    // 1 | c | 1 | c |
    //   +---+   +---+
    //         2 | z |
    //   +---+   +---+
    // 2 | d | 3 | d |
    //   +---+   +---+

    while (o < lines[0].size () && lines[0][o].l == ~0) {
      o += 1;
    }

    // Skip any unmatched lines at this point in the new file.  These
    // are inserts.

    while (n < lines[1].size () && lines[1][n].l == ~0) {
      n += 1;
    }

    // When we get here, we know that we're dealing with matching
    // lines (or the end of one or both files).  We're done with this
    // pass, if we've reached the end of either file.

    if (lines[0].size () <= o || lines[1].size () <= n) {
      break;
    }

    // We now know that we've got a pair of matching lines, the start
    // of a matching block.  If the new file's line number is what we
    // expect, then the matched pair is unmoved, and we can skip over
    // the pair.  (If this is really the start of a block of more than
    // 1 lines, we'll handle it one pair at a time.)

    if (lines[0][o].l == n) {
      o += 1;
      n += 1;
      continue;
    }

    // If, on the other hand, we don't expect this [new] line number,
    // then this matching block has been moved.  We need to turn the
    // matched pair into an *unmatched* pair, one of deletes, and one
    // of inserts.  First, though, we need to know how big this block
    // is, and how far it was moved....

    // Remember where we were when we started.

    unsigned oOld = o;
    unsigned nOld = n;

    // Remember where the block came from.

    unsigned nNew = lines[0][o].l;

    // Find the end of this matched pair.

    for (n = nNew; o < lines[0].size () && n < lines[1].size (); o += 1, n += 1) {
      if (lines[0][o].l != n) {
        break;
      }
    }

    // When we get here:
    // * oOld..o is the old file's block,
    // * nNew..n is the new file's *moved* block, and
    // * nOld is where we originally expected the new block from.
 
    // We can now calculate the length of the block...
    
    unsigned bSize = o - oOld;  // (or n - nNew)

    // ... and we can calculate how far the block moved:

    unsigned bMove = nOld < nNew ? nNew - nOld : nOld - nNew;

    // The larger of the two will become our delete, and the smaller
    // will become our insert.

    //    Old     New
    //   +---+   +---+
    // 0 | a | 0 | b |
    //   +---+ 1 | b |
    // 1 | b |   +---+
    // 2 | b | 2 | a |
    //   +---+   +---+
    // 3 | c | 3 | c |
    //   +---+   +---+

    // (1) Delete a@0, insert a@2, or
    // (2) insert bb@0, delete bb@1?

    // In (1), bSize (of a) is 1-0 or 1, the size of a@0, and bMove
    // (of a) is 2-0 or 2, the size of b@0.  We choose to delete a@0,
    // and reinsert a@2.

    // if (bSize <= bMove) {
      for (unsigned a = oOld; a < o; a += 1) {
        lines[0][a].l = ~0;
      }
      for (unsigned d = nNew; d < n; d += 1) {
        lines[1][d].l = ~0;
      }
    // } else {
      // for (unsigned a = oOld; a < o; a += 1) {
      //   lines[0][a].l = ~0;
      // }
      // for (unsigned d = nNew; d < n; d += 1) {
      //   lines[1][d].l = ~0;
      // }
    // }

    // Continue from where we left off.

    n = nOld;
  }
  nTotalMatchedLines -= nMatchedLines;
  nTotalMatchedBlocks -= nMatchedBlocks;

  // Let's see the intermediate results.

  if (0 < optVerbose) {
    fprintf (stderr,
             " found %u/%u matched lines/blocks (%u/%u totals).\n",
             nMatchedLines,
             nMatchedBlocks,
             nTotalMatchedLines,
             nTotalMatchedBlocks
            );
    fflush (stderr);

    if (2 < optVerbose) {
      for (int f = 0; f < 2; f += 1) {
        fprintf (stderr, "# file #%d\n", f + 1);
        fflush (stderr);
        for (int l = 0; l < lines[f].size (); l += 1) {
          fprintf (stderr, "#   [%d] ", l);
          fflush (stderr);
          lines[f][l].Dump (stderr);
        }
      }
    }
  }
}

// Perform Pass #6, in which we create the desired output.

void pass6c ();
void pass6e ();
void pass6i ();
void pass6n ();
void pass6r ();
void pass6s ();
void pass6u ();
void pass6y ();

void pass6 ()
{

  switch (diffFormat) {
  case Context:
    pass6c ();
    break;

  case EdScript:
    pass6e ();
    break;

  case IfThenElse:
    pass6i ();
    break;

  case Normal:
    pass6n ();
    break;

  case RCS:
    pass6r ();
    break;

  case SideBySide:
    pass6y ();
    break;

  case Unified:
    pass6u ();
    break;

  case Default:
  default:
    pass6c ();
    break;
  }
}

template<typename T> T min (T l, T r) {
  if (l < r) {
    return l;
  }
  return r;
}

template<typename T> T max (T l, T r) {
  if (l < r) {
    return r;
  }
  return l;
}

void pass6c () {
  if (0 < optVerbose) {
    fprintf (stderr, "# Pass #6c (walking the differences)...\n");
    fflush (stderr);
  }

  nMatchedLines = 0;

  // Write the header.

  fprintf (stdout, "*** %s\t%s\n", files[0], "0000-00-00 00:00:00.000000000 +0000");
  fprintf (stdout, "--- %s\t%s\n", files[1], "0000-00-00 00:00:00.000000000 +0000");

  // Starting at the top of both files,...

  unsigned o = 0;
  unsigned n = 0;

  while (o < lines[0].size () || n < lines[1].size ()) {

    // Find the bounds of the current contextual 'window'.

    // If we aren't looking at a delete or an insert, we're not at the
    // start of a context window.

    if (lines[0][o].l != ~0 && lines[1][n].l != ~0) {
      o += 1;
      n += 1;
      continue;
    }

    // We're now looking at at least 1 delete or insert, so a window
    // begins here (or, rather, optCopyContext lines earlier).
    
    int boOldWindow = max (0, int (o) - int (optCopyContext));
    int boNewWindow = max (0, int (n) - int (optCopyContext));

    if (1 < optVerbose) {
      fprintf (stderr, "boOldWindow = %d, boNewWindow = %d\n", boOldWindow, boNewWindow);
      fflush (stderr);
    }

    // Look for the end of the current window.

    int eoOldWindow;
    int eoNewWindow;

    bool widenWindow = true;
    while (widenWindow && o < lines[0].size () || n < lines[1].size ()) {

      // Skip over the delete(s) and/or insert(s) defining the current
      // context.

      while (o < lines[0].size () && lines[0][o].l == ~0) {
        o += 1;
      }
      while (n < lines[1].size () && lines[1][n].l == ~0) {
        n += 1;
      }

      // This context will end optCopyContext matched lines past the
      // last set of deletes or inserts we find.

      widenWindow = false;
      for (unsigned l = 0; !widenWindow && l < optCopyContext; l += 1) {
        if (o < lines[0].size ()) {
          widenWindow |= lines[0][o].l == ~0;
          o += 1;
        }
        if (n < lines[1].size ()) {
          widenWindow |= lines[1][n].l == ~0;
          n += 1;
        }
      }

      eoOldWindow = o;
      eoNewWindow = n;

      // However, since another window could follow, we look
      // optCopyContext ahead for another delete or insert.

      if (!widenWindow) {
        for (unsigned l = 0; l < (2 * optCopyContext + 1); l += 1) {
          if (o < lines[0].size ()) {
            widenWindow |= lines[0][o].l == ~0;
            o += 1;
          }
          if (n < lines[1].size ()) {
            widenWindow |= lines[1][n].l == ~0;
            n += 1;
          }
        }
      }
    }

    // We've found the end of the window.

    if (1 < optVerbose) {
      fprintf (stderr, "eoOldWindow = %d, eoNewWindow = %d\n", eoOldWindow, eoNewWindow);
      fflush (stderr);
    }

    // Now, reset ourselves to the start of the window, and walk it to
    // produce the output.  We'll walk it twice, first for the old
    // file part, then for the new file part.

    fprintf (stdout, "***************\n");
    fprintf (stdout, "*** %d,%d ****\n", boOldWindow + 1, eoOldWindow);

    o = boOldWindow;
    n = boNewWindow;

    while (o < eoOldWindow || n < eoNewWindow) {

      // Find any unmatched lines at this point in the old file.  These
      // are deletes.

      unsigned boDeletes = o;
      while (o < eoOldWindow && lines[0][o].l == ~0) {
        o += 1;
      }

      // Find any unmatched lines at this point in the new file.  These
      // are inserts.

      unsigned boInserts = n;
      while (n < eoNewWindow && lines[1][n].l == ~0) {
        n += 1;
      }

      // We've got deleted line(s) from boDeletes .. o.
      // We've got deleted line(s) from boInserts .. n.

      unsigned nDeletes = o - boDeletes;
      unsigned nInserts = n - boInserts;

      // We've got...

      if (nDeletes && nInserts) {

        // ... deletes and inserts.

        for (unsigned l = boDeletes; l < o; l += 1) {
          fprintf (stdout, "! %s\n", lines[0][l].line->c_str ());
        }
      } else if (nDeletes) {

        //  ... just deletes.

        for (unsigned l = boDeletes; l < o; l += 1) {
          fprintf (stdout, "- %s\n", lines[0][l].line->c_str ());
        }
      }

      // When we get here, we're dealing with matching lines.

      fprintf (stdout, "  %s\n", lines[0][o].line->c_str ());
      o += 1;
      n += 1;
    }

    fprintf (stdout, "--- %d,%d ----\n", boNewWindow + 1, eoNewWindow);

    o = boOldWindow;
    n = boNewWindow;

    while (o < eoOldWindow && n < eoNewWindow) {

      // Find any unmatched lines at this point in the old file.  These
      // are deletes.

      unsigned boDeletes = o;
      while (o < eoOldWindow && lines[0][o].l == ~0) {
        o += 1;
      }

      // Find any unmatched lines at this point in the new file.  These
      // are inserts.

      unsigned boInserts = n;
      while (n < eoNewWindow && lines[1][n].l == ~0) {
        n += 1;
      }

      // We've got deleted line(s) from boDeletes .. o.
      // We've got deleted line(s) from boInserts .. n.

      unsigned nDeletes = o - boDeletes;
      unsigned nInserts = n - boInserts;

      // We've got...

      if (nDeletes && nInserts) {

        // ... deletes and inserts.

        for (unsigned l = boInserts; l < n; l += 1) {
          fprintf (stdout, "! %s\n", lines[1][l].line->c_str ());
        }
      } else if (nInserts) {

        //  ... just inserts.

        for (unsigned l = boInserts; l < n; l += 1) {
          fprintf (stdout, "+ %s\n", lines[1][l].line->c_str ());
        }
      }

      // When we get here, we're dealing with matching lines.

      fprintf (stdout, "  %s\n", lines[1][n].line->c_str ());
      o += 1;
      n += 1;
    }

    // Reset to the end of the current window.

    o = eoOldWindow;
    n = eoNewWindow;
  }
}

void pass6e () {
  fprintf (stderr, "pass6e() is unimplemented!\n");
  fflush (stderr);
}

void pass6i () {
  fprintf (stderr, "pass6i() is unimplemented!\n");
  fflush (stderr);
}

void pass6n ()
{
  if (0 < optVerbose) {
    fprintf (stderr, "# Pass #6n (walking the differences)...\n");
    fflush (stderr);
  }

  nMatchedLines = 0;

  // Write the header.  (None for normal.)

  // Starting at the top of both files,...

  unsigned o = 0;
  unsigned n = 0;

  while (o < lines[0].size () || n < lines[1].size ()) {

    // Find any unmatched lines at this point in the old file.  These
    // are deletes.

    unsigned boDeletes;
    for (boDeletes = o;
         o < lines[0].size () && lines[0][o].l == ~0;
         o += 1
        )
    {}

    // Find any unmatched lines at this point in the new file.  These
    // are inserts.

    unsigned boInserts;
    for (boInserts = n;
         n < lines[1].size () && lines[1][n].l == ~0;
         n += 1
        )
    {}

    // We've got deleted line(s) from boDeletes .. o.
    // We've got deleted line(s) from boInserts .. n.

    unsigned nDeletes = o - boDeletes;
    unsigned nInserts = n - boInserts;

    // We've got...

    if (nDeletes && nInserts) {

      // ... deletes and inserts.

      if (1 < nDeletes && 1 < nInserts) {
        fprintf (stdout, "%d,%dc%d,%d\n", boDeletes + 1, o, boInserts + 1, n);
      } else if (1 < nDeletes) {
        fprintf (stdout, "%d,%dc%d\n", boDeletes + 1, o, boInserts + 1);
      } else if (1 < nInserts) {
        fprintf (stdout, "%dc%d,%d\n", boDeletes + 1, boInserts + 1, n);
      } 
      for (unsigned l = boDeletes; l < o; l += 1) {
        fprintf (stdout, "< %s\n", lines[0][l].line->c_str ());
      }
      fprintf (stdout, "---\n");
      for (unsigned l = boInserts; l < n; l += 1) {
        fprintf (stdout, "> %s\n", lines[1][l].line->c_str ());
      }
    } else if (nDeletes) {

      //  ... just deletes.

      if (1 < nDeletes) {
        fprintf (stdout, "%d,%dd%d\n", boDeletes + 1, o, n);
      } else {
        fprintf (stdout, "%dd%d\n", boDeletes + 1, n);
      }
      for (unsigned l = boDeletes; l < o; l += 1) {
        fprintf (stdout, "< %s\n", lines[0][l].line->c_str ());
      }
    } else if (nInserts) {

      //  ... just inserts.

      if (1 < nInserts) {
        fprintf (stdout, "%da%d,%d\n", o, boInserts + 1, n);
      } else {
        fprintf (stdout, "%da%d\n", o, boInserts + 1);
      }
      for (unsigned l = boInserts; l < n; l += 1) {
        fprintf (stdout, "> %s\n", lines[1][l].line->c_str ());
      }
    }

    // When we get here, we're dealing with matching lines.

    o += 1;
    n += 1;
  }
}

void pass6r () {
  fprintf (stderr, "pass6r() is unimplemented!\n");
  fflush (stderr);
}

void pass6s () {
  fprintf (stderr, "pass6s() is unimplemented!\n");
  fflush (stderr);
}

void pass6u ()
{
  if (0 < optVerbose) {
    fprintf (stderr, "# Pass #6u (walking the differences)...\n");
    fflush (stderr);
  }

  nMatchedLines = 0;

  // Write the header.

  fprintf (stdout, "--- %s\t%s\n", files[0], "0000-00-00 00:00:00.000000000 +0000");
  fprintf (stdout, "+++ %s\t%s\n", files[1], "0000-00-00 00:00:00.000000000 +0000");
  fprintf (stdout, "@@ -1,%d +1,%d @@\n", lines[0].size (), lines[1].size ());

  // Starting at the top of both files,...

  unsigned o = 0;
  unsigned n = 0;

  while (o < lines[0].size () || n < lines[1].size ()) {

    // Find any unmatched lines at this point in the old file.  These
    // are deletes.

    unsigned boDeletes;
    for (boDeletes = o;
         o < lines[0].size () && lines[0][o].l == ~0;
         o += 1
        )
    {}

    // Find any unmatched lines at this point in the new file.  These
    // are inserts.

    unsigned boInserts;
    for (boInserts = n;
         n < lines[1].size () && lines[1][n].l == ~0;
         n += 1
        )
    {}

    // We've got deleted line(s) from boDeletes .. o.
    // We've got deleted line(s) from boInserts .. n.

    unsigned nDeletes = o - boDeletes;
    unsigned nInserts = n - boInserts;

    // We've got...

    if (nDeletes && nInserts) {

      // ... deletes and inserts.

      for (unsigned l = boDeletes; l < o; l += 1) {
        fprintf (stdout, "-%s\n", lines[0][l].line->c_str ());
      }
      for (unsigned l = boInserts; l < n; l += 1) {
        fprintf (stdout, "+%s\n", lines[1][l].line->c_str ());
      }
    } else if (nDeletes) {

      //  ... just deletes.

      for (unsigned l = boDeletes; l < o; l += 1) {
        fprintf (stdout, "-%s\n", lines[0][l].line->c_str ());
      }
    } else if (nInserts) {

      //  ... just inserts.

      for (unsigned l = boInserts; l < n; l += 1) {
        fprintf (stdout, "+%s\n", lines[1][l].line->c_str ());
      }
    }

    // When we get here, we're dealing with matching lines.

    fprintf (stdout, " %s\n", lines[0][o].line->c_str ());
    o += 1;
    n += 1;
  }
}

void pass6y ()
{
  if (0 < optVerbose) {
    fprintf (stderr, "# Pass #6y (walking the differences)...\n");
    fflush (stderr);
  }

  // Write the header.

  // None for side-by-side.

  // Starting at the top of both files,...

  int columnWidth = (optPageWidth - 7) / 2;

  unsigned o = 0;
  unsigned n = 0;

  while (o < lines[0].size () || n < lines[1].size ()) {

    // Find any unmatched lines at this point in the old file.  These
    // are deletes.

    unsigned boDeletes = o;
    while (o < lines[0].size () && lines[0][o].l == ~0) {
      o += 1;
    }

    // Find any unmatched lines at this point in the new file.  These
    // are inserts.

    unsigned boInserts = n;
    while (n < lines[1].size () && lines[1][n].l == ~0) {
      n += 1;
    }

    // We've got deleted line(s) from boDeletes .. o.
    // We've got deleted line(s) from boInserts .. n.

    unsigned nDeletes = o - boDeletes;
    unsigned nInserts = n - boInserts;

    // We've got...

    while (boDeletes < o && boInserts < n) {

      // ... deletes and inserts.

      fprintf (stdout,
               "%-*s | %s\n",
               columnWidth,
               lines[0][boDeletes].line->substr (0, columnWidth).c_str (),
               lines[1][boInserts].line->substr (0, columnWidth).c_str ()
              );
      boDeletes += 1;
      boInserts += 1;
    }

    while (boDeletes < o) {

      //  ... just deletes.

      fprintf (stdout,
               "%-*s <\n",
               columnWidth,
               lines[0][boDeletes].line->substr (0, columnWidth).c_str ()
              );
      boDeletes += 1;
    }

    while (boInserts < n) {

      //  ... just inserts.

      fprintf (stdout,
               "%-*s > %s\n",
               columnWidth,
               "",
               lines[1][boInserts].line->substr (0, columnWidth).c_str ()
              );
      boInserts += 1;
    }

    // When we get here, we're dealing with matching lines.

    fprintf (stdout,
             "%-*s   %s\n",
             columnWidth,
             lines[0][o].line->substr (0, columnWidth).c_str (),
             lines[1][n].line->substr (0, columnWidth).c_str ()
            );
    o += 1;
    n += 1;
  }
}
