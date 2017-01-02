#include <assert.h>
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
#include <getopt.h>

enum DiffFormat {
  opt_D_IfThenElse,
  opt___Normal,
  opt_c_Context,
  opt_e_EdScript,
  opt_n_RCS,
  opt_u_Unified,
  opt_y_SideBySide
};

DiffFormat opt___OutputFormat = opt___Normal;

bool       opt_B_IgnoreBlankLines = false;
unsigned   opt_C_LinesOfCopyContext = 3;
string     opt_D_IfdefName;
bool       opt_E_IgnoreTabExpansionChanges = false;
string     opt_F_ShowMostRecentLineMatching;
string     opt_I_IgnoreLinesMatching;
bool       opt_N_TreatAbsentFilesAsEmpty = false;
string     opt_S_StartWithFileMatching;
bool       opt_T_PrependATab = false;
unsigned   opt_U_LinesOfUnifiedContext = 3;
unsigned   opt_W_MaxPrintColumns = 130;
string     opt_X_ExcludeFilesMatching;
bool       opt_a_TreatAllFilesAsText = false;
bool       opt_b_IgnoreWhitespaceChanges = false;
bool       opt_d_TryToFindMinimalChanges = false;
bool       opt_i_IgnoreCaseDifferences = false;
bool       opt_l_PaginateWithPr = false;
bool       opt_p_ShowChangedCFunction = false;
bool       opt_q_OutputOnlyIfFilesDiffer = false;
bool       opt_r_RecursivelyCompareSubdirectories = false;
bool       opt_s_ReportWhenFilesAreSame = false;
bool       opt_t_ExpandTabs = false;
bool       opt_v_ShowVersionInfo = false;
bool       opt_w_IgnoreAllWhitespace = false;
string     opt_x_IgnoreFilesMatching;

unsigned opt_d_Debug = 0;

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
    for (list<unsigned>::iterator i = copies[0].begin ();
         i != copies[0].end ();
         i++
        )
    {
      fprintf (out, "%3u ", *i);
    }
    fprintf (out, "},{ ");
    for (list<unsigned>::iterator i = copies[1].begin ();
         i != copies[1].end ();
         i++
        )
    {
      fprintf (out, "%3u ", *i);
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
    fprintf (out, "[%3d] ", l);
    line->Dump (out);
  }
  Line *line;
  unsigned l;
};

typedef map<string, Line *> MapStringToLinePtr;
typedef vector<LinePtr> VectorLinePtr;

// Our name.

char const *ARGV0 = "sdiff";

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

void getopts (int argc, char const *const argv[]);

void pass1 ();
void pass2 ();
void pass3 ();
void pass4 ();
void pass5 ();
void pass6 ();

void DumpFileLine (unsigned f, unsigned l) {
  if (l < 0 || lines[f].size () <= l) {
    return;
  }

  bool badMatch = lines[f][l].l != ~0u && lines[!f][lines[f][l].l].l != l;
  fprintf (stderr,
           "#   %s[%3d]%s",
           f == 0 ? "0" : "    1",
           l,
           badMatch ? "?" : " "
          );
  fflush (stderr);
  lines[f][l].Dump (stderr);
}

void DumpFileLines (unsigned f) {
  for (int l = 0; l < lines[f].size (); l += 1) {
    DumpFileLine (f, l);
  }
}

void DumpFilesLines () {
  for (int f = 0; f < 2; f += 1) {
    fprintf (stderr, "# file #%d\n", f + 1);
    fflush (stderr);
    DumpFileLines (f);
  }
}

int main (int argc, char const *const argv[])
{
  ARGV0 = const_cast<char const *> (argv[0]);

  if (0 < opt_d_Debug) {
    fprintf (stderr, "#");
    for (int a = 0; a < argc; a += 1) {
      fprintf (stderr, " %s", argv[a]);
    }
    fprintf (stderr, "\n");
    fflush (stderr);
  }

  getopts (argc, argv);

  if (opt_v_ShowVersionInfo) {
    fprintf
      (stderr,
       "%s () 0.0.1\n"
       "Copyright (C) 2008 Sidney R Maxwell III\n"
       "\n"
       "This program comes with NO WARRANTY, to the extent permitted by law.\n"
       "You may redistribute copies of this program\n"
       "under the terms of the GNU General Public License.\n"
       "For more information about these matters, see the file named COPYING.\n"
       "\n"
       "Written by Sid Maxwell.\n",
       ARGV0
      );
    exit (0);
  }

  // Setup a matched, beginning-of-file sentinel 'line'.

  Line *BoFSentinel = new Line ("<bof>");
  unsigned o = 0;
  unsigned n = 0;
  lines[0].push_back (BoFSentinel);
  lines[1].push_back (BoFSentinel);
  lines[0][o].l = n;
  lines[1][n].l = o;

  // Read the old [0] file, and the new [1] file.

  for (int a = optind, n = 0; a < argc && n < 2; a += 1, n += 1) {
    files[n] = argv[a];

    if (FILE *f = fopen (files[n], "r")) {
      if (0 < opt_d_Debug) {
        fprintf (stderr, "# Reading %s...", files[n]);
        fflush (stderr);
      }

      char buffer[1024];
      unsigned nUniq = 0;

      while (fgets (buffer, sizeof (buffer), f)) {
        string text (buffer);

        if (0 < text.size ()) {
          if (text[text.size () - 1] == '\n') {
            text.erase (text.end () - 1);
            if (0 < text.size ()) {
              if (text[text.size () - 1] == '\r') {
                text.erase (text.end () - 1);
              }
            }
          }
        }

        Line *line = table[text];

        if (!line) {
          table[text] = line = new Line (text);
          nUniq += 1;
        }
        line->copies[n].push_back (lines[n].size ());
        lines[n].push_back (line);
      }

      if (0 < opt_d_Debug) {
        fprintf (stderr, " %u lines, %u unique.\n", lines[n].size (), nUniq);
        fflush (stderr);
      }
    } else {
      fprintf (stderr,
               "%s: Unable to open %s!  Exiting....\n",
               ARGV0,
               files[n]
              );
      fflush (stderr);
      perror (ARGV0);
      exit (1);
    }
  }

  // Setup a matched, end-of-file sentinel 'line'.

  Line *EoFSentinel = new Line ("<eof>");
  o = lines[0].size ();
  n = lines[1].size ();
  lines[0].push_back (EoFSentinel);
  lines[1].push_back (EoFSentinel);
  lines[0][o].l = n;
  lines[1][n].l = o;

  if (0 < opt_d_Debug) {
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

// Usage: diff [OPTION]... FILES
// Compare files line by line.
// 
//   --GTYPE-group-format=GFMT  Similar, but format GTYPE input groups with GFMT.
//   --LTYPE-line-format=LFMT  Similar, but format LTYPE input lines with LFMT.
//     LTYPE is `old', `new', or `unchanged'.  GTYPE is LTYPE or `changed'.
//     GFMT may contain:
//       %<  lines from FILE1
//       %>  lines from FILE2
//       %=  lines common to FILE1 and FILE2
//       %[-][WIDTH][.[PREC]]{doxX}LETTER  printf-style spec for LETTER
//         LETTERs are as follows for new group, lower case for old group:
//           F  first line number
//           L  last line number
//           N  number of lines = L-F+1
//           E  F-1
//           M  L+1
//     LFMT may contain:
//       %L  contents of line
//       %l  contents of line, excluding any trailing newline
//       %[-][WIDTH][.[PREC]]{doxX}n  printf-style spec for input line number
//     Either GFMT or LFMT may contain:
//       %%  %
//       %c'C'  the single character C
//       %c'\OOO'  the character with octal code OOO
//
//   --brief  -q  Output only whether files differ.
//   --context[=NUM]  -c  -C NUM  Output NUM (default 3) lines of copied context.
//   --debug[=NUM]  -d NUM  Set dubugging output level to NUM (default 1).
//   --ed  -e  Output an ed script.
//   --exclude-from=FILE  -X FILE  Exclude files that match any pattern in FILE.
//   --exclude=PAT  -x PAT  Exclude files that match PAT.
//   --expand-tabs  -t  Expand tabs to spaces in output.
//   --from-file=FILE1  Compare FILE1 to all operands.  FILE1 can be a directory.
//   --help  Output this help.
//   --horizon-lines=NUM  Keep NUM lines of the common prefix and suffix.
//   --ifdef=NAME  -D NAME  Output merged file to show `#ifdef NAME' diffs.
//   --ignore-all-space  -w  Ignore all white space.
//   --ignore-blank-lines  -B  Ignore changes whose lines are all blank.
//   --ignore-case  -i  Ignore case differences in file contents.
//   --ignore-file-name-case  Ignore case when comparing file names.
//   --ignore-matching-lines=RE  -I RE  Ignore changes whose lines all match RE.
//   --ignore-space-change  -b  Ignore changes in the amount of white space.
//   --ignore-tab-expansion  -E  Ignore changes due to tab expansion.
//   --initial-tab  -T  Make tabs line up by prepending a tab.
//   --label LABEL  Use LABEL instead of file name.
//   --left-column  Output only the left column of common lines.
//   --line-format=LFMT  Similar, but format all input lines with LFMT.
//   --minimal  -d  Try hard to find a smaller set of changes.
//   --new-file  -N  Treat absent files as empty.
//   --no-ignore-file-name-case  Consider case when comparing file names.
//   --normal  Output a normal diff.
//   --paginate  -l  Pass the output through `pr' to paginate it.
//   --rcs  -n  Output an RCS format diff.
//   --recursive  -r  Recursively compare any subdirectories found.
//   --report-identical-files  -s  Report when two files are the same.
//   --show-c-function  -p  Show which C function each change is in.
//   --show-function-line=RE  -F RE  Show the most recent line matching RE.
//   --side-by-side  -y  Output in two columns.
//   --speed-large-files  Assume large files and many scattered small changes.
//   --starting-file=FILE  -S FILE  Start with FILE when comparing directories.
//   --strip-trailing-cr  Strip trailing carriage return on input.
//   --suppress-common-lines  Do not output common lines.
//   --text  -a  Treat all files as text.
//   --to-file=FILE2  Compare all operands to FILE2.  FILE2 can be a directory.
//   --unidirectional-new-file  Treat absent first files as empty.
//   --unified[=NUM]  -u  -U NUM  Output NUM (default 3) lines of unified context.
//   --version  -v  Output version info.
//   --width=NUM  -W NUM  Output at most NUM (default 130) print columns.
// 
// FILES are `FILE1 FILE2' or `DIR1 DIR2' or `DIR FILE...' or `FILE... DIR'.
// If --from-file or --to-file is given, there are no restrictions on FILES.
// If a FILE is `-', read standard input.
// 
// Report bugs to <bug-gnu-utils@gnu.org>.

void getopts (int argc, char const *const argv[])
{
  while (1) {
    int option_index = 0;
    static option long_options[] = {
      { "brief",                        0, 0, 'q' }, // Output only whether files differ.
      { "context",                      2, 0, 'c' }, // Output NUM (default 3) lines of copied context.
      { "debug",                        2, 0, 'd' }, // Debug level NUM (default 1).
      { "ed",                           0, 0, 'e' }, // Output an ed script.
      { "exclude-from",                 1, 0,  0  }, // Exclude files that match any pattern in FILE.
      { "exclude",                      1, 0, 'x' }, // Exclude files that match PAT.
      { "expand-tabs",                  0, 0, 't' }, // Expand tabs to spaces in output.
      { "from-file",                    1, 0,  0  }, // Compare FILE1 to all operands. FILE1 can be a directory.
      { "help",                         0, 0, 'h' }, // Output this help.
      { "horizon-lines",                1, 0,  0  }, // Keep NUM lines of the common prefix and suffix.
      { "ifdef",                        1, 0, 'D' }, // Output merged file to show `#ifdef NAME' diffs.
      { "ignore-all-space",             0, 0, 'w' }, // Ignore all white space.
      { "ignore-blank-lines",           0, 0, 'B' }, // Ignore changes whose lines are all blank.
      { "ignore-case",                  0, 0, 'i' }, // Ignore case differences in file contents.
      { "ignore-file-name-case",        0, 0,  0  }, // Ignore case when comparing file names.
      { "ignore-matching-lines",        1, 0, 'I' }, // Ignore changes whose lines all match RE.
      { "ignore-space-change",          0, 0, 'b' }, // Ignore changes in the amount of white space.
      { "ignore-tab-expansion",         0, 0, 'E' }, // Ignore changes due to tab expansion.
      { "initial-tab",                  0, 0, 'T' }, // Make tabs line up by prepending a tab.
      { "label",                        1, 0,  0  }, // Use LABEL instead of file name.
      { "left-column",                  0, 0,  0  }, // Output only the left column of common lines.
      { "line-format",                  1, 0,  0  }, // Similar, but format all input lines with LFMT.
      { "minimal",                      0, 0,  0  }, // Try hard to find a smaller set of changes.
      { "new-file",                     0, 0, 'N' }, // Treat absent files as empty.
      { "no-ignore-file-name-case",     0, 0,  0  }, // Consider case when comparing file names.
      { "normal",                       0, 0,  0  }, // Output a normal diff.
      { "paginate",                     0, 0, 'l' }, // Pass the output through `pr' to paginate it.
      { "rcs",                          0, 0, 'n' }, // Output an RCS format diff.
      { "recursive",                    0, 0, 'r' }, // Recursively compare any subdirectories found.
      { "report-identical-files",       0, 0, 's' }, // Report when two files are the same.
      { "show-c-function",              0, 0, 'p' }, // Show which C function each change is in.
      { "show-function-line",           1, 0, 'F' }, // Show the most recent line matching RE.
      { "side-by-side",                 0, 0, 'y' }, // Output in two columns.
      { "speed-large-files",            0, 0,  0  }, // Assume large files and many scattered small changes.
      { "starting-file",                1, 0, 'S' }, // Start with FILE when comparing directories.
      { "strip-trailing-cr",            0, 0,  0  }, // Strip trailing carriage return on input.
      { "suppress-common-lines",        0, 0,  0  }, // Do not output common lines.
      { "text",                         0, 0, 'a' }, // Treat all files as text.
      { "to-file",                      0, 0,  0  }, // Compare all operands to FILE2.  FILE2 can be a directory.
      { "unidirectional-new-file",      0, 0,  0  }, // Treat absent first files as empty.
      { "unified",                      2, 0, 'u' }, // Output NUM (default 3) lines of unified context.
      { "version",                      0, 0, 'v' }, // Output version info.
      { "width",                        1, 0, 'W' }, // Output at most NUM (default 130) print columns.
      { 0,				0, 0,   0 }
    };

    char const *short_options =
      "B"                       // Ignore changes whose lines are all blank.
      "C:"                      // Output NUM (default 3) lines of copied context.
      "D:"                      // Output merged file to show `#ifdef NAME' diffs.
      "E"                       // Ignore changes due to tab expansion.
      "F:"                      // Show the most recent line matching RE.
      "I:"                      // Ignore changes whose lines all match RE.
      "N"                       // Treat absent files as empty.
      "S:"                      // Start with FILE when comparing directories.
      "T"                       // Make tabs line up by prepending a tab.
      "U:"                      // Output NUM (default 3) lines of unified context.
      "W:"                      // Output at most NUM (default 130) print columns.
      "X:"                      // Exclude files that match any pattern in FILE.
      "a"                       // Treat all files as text.
      "b"                       // Ignore changes in the amount of white space.
      "c"                       // Output NUM (default 3) lines of copied context.
      "d:"                      // Set dubugging output level to NUM (default 1).
      "e"                       // Output an ed script.
      "i"                       // Ignore case differences in file contents.
      "l"                       // Pass the output through `pr' to paginate it.
      "n"                       // Output an RCS format diff.
      "p"                       // Show which C function each change is in.
      "q"                       // Output only whether files differ.
      "r"                       // Recursively compare any subdirectories found.
      "s"                       // Report when two files are the same.
      "t"                       // Expand tabs to spaces in output.
      "u"                       // Output NUM (default 3) lines of unified context.
      "v"                       // Output version info.
      "w"                       // Ignore all white space.
      "x:"                      // Exclude files that match PAT.
      "y"                       // Output in two columns.
      ;

    int c =
      getopt_long
        (argc,
         const_cast<char *const *> (argv),
         short_options,
         long_options,
         &option_index
        );

    switch (c) {
    case -1:
      return;

    case 0:
      if (strcmp (long_options[option_index].name, "normal") == 0) {
        opt___OutputFormat = opt___Normal;
        break;
      }

      if (optarg) {
        fprintf (stderr,
                 "%s: option --%s[=%s] is unsupported!  Ignoring...\n",
                 ARGV0,
                 long_options[option_index].name,
                 optarg
                );
      } else {
        fprintf (stderr,
                 "%s: option --%s is unsupported!  Ignoring...\n",
                 ARGV0,
                 long_options[option_index].name
                );
      }
      break;
    case 'B':
      opt_B_IgnoreBlankLines = true;
      break;
    case 'C':
      {
        unsigned linesOfContext = 3;
        if (optarg) {
          if (sscanf (optarg, "%u", &linesOfContext) != 1) {
            fprintf (stderr,
                     "-C <NUM> or --context[=<NUM>] (%d)\n",
                     opt_C_LinesOfCopyContext
                    );
            exit (1);
          }
        }
        opt_C_LinesOfCopyContext = linesOfContext;
      }
      break;
    case 'D':
      opt_D_IfdefName = optarg;
      break;
    case 'E':
      opt_E_IgnoreTabExpansionChanges = true;
      break;
    case 'F':
      opt_F_ShowMostRecentLineMatching = optarg;
      break;
    case 'I':
      opt_I_IgnoreLinesMatching = optarg;
      break;
    case 'N':
      opt_N_TreatAbsentFilesAsEmpty = true;
      break;
    case 'S':
      opt_S_StartWithFileMatching = optarg;
      break;
    case 'T':
      opt_T_PrependATab = true;
      break;
    case 'U':
      {
        unsigned linesOfContext = 3;
        if (optarg) {
          if (sscanf (optarg, "%u", &linesOfContext) != 1) {
            fprintf (stderr,
                     "-U <NUM> or --unified[=<NUM>] (%d)\n",
                     opt_U_LinesOfUnifiedContext
                    );
            exit (1);
          }
        }
        opt_U_LinesOfUnifiedContext = linesOfContext;
      }
      break;
    case 'W':
      {
        unsigned maxPrintColumns = 130;
        if (optarg) {
          if (sscanf (optarg, "%u", &maxPrintColumns) != 1) {
            fprintf (stderr,
                     "-W <NUM> or --width[=<NUM>] (%d)\n",
                     opt_W_MaxPrintColumns
                    );
            exit (1);
          }
        }
        opt_W_MaxPrintColumns = maxPrintColumns;
      }
      break;
    case 'X':
      opt_X_ExcludeFilesMatching = optarg;
      break;
    case 'a':
      opt_a_TreatAllFilesAsText = true;
      break;
    case 'b':
      opt_b_IgnoreWhitespaceChanges = true;
      break;
    case 'c':
      opt___OutputFormat = opt_c_Context;
      break;
    case 'd':
      {
        unsigned debugLevel = 1;
        if (optarg) {
          if (sscanf (optarg, "%u", &debugLevel) != 1) {
            fprintf (stderr,
                     "-d <NUM> or --debug[=<NUM>] (%d)\n",
                     opt_d_Debug
                    );
            exit (1);
          }
        }
        opt_d_Debug = debugLevel;
      }
      break;
    case 'e':
      opt___OutputFormat = opt_e_EdScript;
      break;
    case 'i':
      opt_i_IgnoreCaseDifferences = true;
      break;
    case 'l':
      opt_l_PaginateWithPr = true;
      break;
    case 'n':
      opt___OutputFormat = opt_n_RCS;
      break;
    case 'p':
      opt_p_ShowChangedCFunction = true;
      break;
    case 'q':
      opt_q_OutputOnlyIfFilesDiffer = true;
      break;
    case 'r':
      opt_r_RecursivelyCompareSubdirectories = true;
      break;
    case 's':
      opt_s_ReportWhenFilesAreSame = true;
      break;
    case 't':
      opt_t_ExpandTabs = true;
      break;
    case 'u':
      opt___OutputFormat = opt_u_Unified;
      break;
    case 'v':
      opt_v_ShowVersionInfo = true;
      break;
    case 'w':
      opt_w_IgnoreAllWhitespace = true;
      break;
    case 'x':
      opt_x_IgnoreFilesMatching = optarg;
      break;
    case 'y':
      opt___OutputFormat = opt_y_SideBySide;
      break;
    case 'h':
    case '?':
      fprintf
        (stderr,
         "Usage: diff [OPTION]... FILES\n"
         "Compare files line by line.\n"
         "\n"
         "  --GTYPE-group-format=GFMT  Similar, but format GTYPE input groups with GFMT.\n"
         "  --LTYPE-line-format=LFMT  Similar, but format LTYPE input lines with LFMT.\n"
         "    LTYPE is `old', `new', or `unchanged'.  GTYPE is LTYPE or `changed'.\n"
         "    GFMT may contain:\n"
         "      %<  lines from FILE1\n"
         "      %>  lines from FILE2\n"
         "      %=  lines common to FILE1 and FILE2\n"
         "      %[-][WIDTH][.[PREC]]{doxX}LETTER  printf-style spec for LETTER\n"
         "        LETTERs are as follows for new group, lower case for old group:\n"
         "          F  first line number\n"
         "          L  last line number\n"
         "          N  number of lines = L-F+1\n"
         "          E  F-1\n"
         "          M  L+1\n"
         "    LFMT may contain:\n"
         "      %L  contents of line\n"
         "      %l  contents of line, excluding any trailing newline\n"
         "      %[-][WIDTH][.[PREC]]{doxX}n  printf-style spec for input line number\n"
         "    Either GFMT or LFMT may contain:\n"
         "      %%  %\n"
         "      %c'C'  the single character C\n"
         "      %c'\\OOO'  the character with octal code OOO\n"
         "\n"
         "  --brief  -q  Output only whether files differ.\n"
         "  --context[=NUM]  -c  -C NUM  Output NUM (default 3) lines of copied context.\n"
         "  --ed  -e  Output an ed script.\n"
         "  --exclude-from=FILE  -X FILE  Exclude files that match any pattern in FILE.\n"
         "  --exclude=PAT  -x PAT  Exclude files that match PAT.\n"
         "  --expand-tabs  -t  Expand tabs to spaces in output.\n"
         "  --from-file=FILE1  Compare FILE1 to all operands.  FILE1 can be a directory.\n"
         "  --help  Output this help.\n"
         "  --horizon-lines=NUM  Keep NUM lines of the common prefix and suffix.\n"
         "  --ifdef=NAME  -D NAME  Output merged file to show `#ifdef NAME' diffs.\n"
         "  --ignore-all-space  -w  Ignore all white space.\n"
         "  --ignore-blank-lines  -B  Ignore changes whose lines are all blank.\n"
         "  --ignore-case  -i  Ignore case differences in file contents.\n"
         "  --ignore-file-name-case  Ignore case when comparing file names.\n"
         "  --ignore-matching-lines=RE  -I RE  Ignore changes whose lines all match RE.\n"
         "  --ignore-space-change  -b  Ignore changes in the amount of white space.\n"
         "  --ignore-tab-expansion  -E  Ignore changes due to tab expansion.\n"
         "  --initial-tab  -T  Make tabs line up by prepending a tab.\n"
         "  --label LABEL  Use LABEL instead of file name.\n"
         "  --left-column  Output only the left column of common lines.\n"
         "  --line-format=LFMT  Similar, but format all input lines with LFMT.\n"
         "  --minimal  -d  Try hard to find a smaller set of changes.\n"
         "  --new-file  -N  Treat absent files as empty.\n"
         "  --no-ignore-file-name-case  Consider case when comparing file names.\n"
         "  --normal  Output a normal diff.\n"
         "  --paginate  -l  Pass the output through `pr' to paginate it.\n"
         "  --rcs  -n  Output an RCS format diff.\n"
         "  --recursive  -r  Recursively compare any subdirectories found.\n"
         "  --report-identical-files  -s  Report when two files are the same.\n"
         "  --show-c-function  -p  Show which C function each change is in.\n"
         "  --show-function-line=RE  -F RE  Show the most recent line matching RE.\n"
         "  --side-by-side  -y  Output in two columns.\n"
         "  --speed-large-files  Assume large files and many scattered small changes.\n"
         "  --starting-file=FILE  -S FILE  Start with FILE when comparing directories.\n"
         "  --strip-trailing-cr  Strip trailing carriage return on input.\n"
         "  --suppress-common-lines  Do not output common lines.\n"
         "  --text  -a  Treat all files as text.\n"
         "  --to-file=FILE2  Compare all operands to FILE2.  FILE2 can be a directory.\n"
         "  --unidirectional-new-file  Treat absent first files as empty.\n"
         "  --unified[=NUM]  -u  -U NUM  Output NUM (default 3) lines of unified context.\n"
         "  --version  -v  Output version info.\n"
         "  --width=NUM  -W NUM  Output at most NUM (default 130) print columns.\n"
         "\n"
         "FILES are `FILE1 FILE2' or `DIR1 DIR2' or `DIR FILE...' or `FILE... DIR'.\n"
         "If --from-file or --to-file is given, there are no restrictions on FILES.\n"
         "If a FILE is `-', read standard input.\n"
         "\n"
         "Report bugs to <bug-gnu-utils@gnu.org>.\n"
        );
      exit (1);
    default:
      fprintf (stderr, "Unhandled option (%c%s)!\n", c, optarg ? optarg : "");
      exit (1);
    }
  }
}

// Perform pass #1, in which we find all explicit matches amongst the
// unique lines.  For every unique line that appears the same number
// of times in both files, we declare the lines to match.

void pass1 ()
{
  if (0 < opt_d_Debug) {
    fprintf (stderr, "# Pass 1 (finding matches amongst unique lines)...");
    fflush (stderr);
  }

  nMatchedLines = 0;

  for (MapStringToLinePtr::iterator i = table.begin ();
       i != table.end ();
       i++
      )
  {
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

        if (2 < opt_d_Debug) {
          DumpFileLine (0, o);
          DumpFileLine (1, n);
        }
      }
    }
  }
  nTotalMatchedLines += nMatchedLines;

  // Let's see the intermediate results.

  if (0 < opt_d_Debug) {
    fprintf (stderr,
             " found %u matched lines (%u total).\n",
             nMatchedLines,
             nTotalMatchedLines
            );
    fflush (stderr);

    if (3 < opt_d_Debug) {
      DumpFilesLines ();
    }
  }
}

// Perform Pass #2, in which we attempt to 'widen' blocks of matched
// lines, by appending lines which are the same in both files.

void pass2 ()
{
  if (0 < opt_d_Debug) {
    fprintf (stderr, "# Pass #2 (spreading matches down)...");
    fflush (stderr);
  }

  nMatchedBlocks = 0;
  nMatchedLines = 0;

  // Starting at the top of the old file,...

  for (unsigned o = 0; o < lines[0].size (); o += 1) {

    if (2 < opt_d_Debug) {
      DumpFileLine (0, o);
    }

    // ... skip any currently unmatched lines...

    if (lines[0][o].l == ~0u) {
      continue;
    }

    // ... until we find an [already] matched line.  Then, skip over
    // the set of 1 or more matched lines, until we reach any
    // following, still-unmatched line.

    unsigned n;
    do {
      n = lines[0][o].l + 1;
      o += 1;

      if (2 < opt_d_Debug) {
        DumpFileLine (0, o);
        DumpFileLine (1, n);
      }
    } while (o < lines[0].size () && lines[0][o].l != ~0u);

    // Now, o - 1 is the line number of the last matched old line, and
    // n - 1 is the line number of the last matched new line.  We want
    // to add new old and new lines, if they match.

    // Finally, attempt to add unmatched lines to the preceeding
    // matched set.

    for (; o < lines[0].size () && lines[0][o].l == ~0u; o += 1, n += 1) {

      // If the [next] pair of old and new lines aren't the same line,
      // we're done.

      if (lines[0][o].line != lines[1][n].line) {
        break;
      }

      // Match up the pair.

      lines[0][o].l = n;
      lines[1][n].l = o;

      // Remove the matched line numbers form the lists.

      Line *line = lines[0][o].line;
      list<unsigned> &oldCopies = line->copies[0];
      list<unsigned>::iterator oL =
        find (oldCopies.begin (), oldCopies.end (), o);
      if (oL != oldCopies.end ()) {
        oldCopies.erase (oL);
      }
      list<unsigned> &newCopies = line->copies[1];
      list<unsigned>::iterator nL =
        find (newCopies.begin (), newCopies.end (), n);
      if (nL != newCopies.end ()) {
        newCopies.erase (nL);
      }

      if (2 < opt_d_Debug) {
        DumpFileLine (0, o);
        DumpFileLine (1, n);
      }
      nMatchedLines += 1;
    }
    nMatchedBlocks += 1;
  }
  nTotalMatchedLines += nMatchedLines;
  nTotalMatchedBlocks += nMatchedBlocks;

  // Let's see the intermediate results.

  if (0 < opt_d_Debug) {
    fprintf (stderr,
             " found %u/%u matched lines/blocks (%u/%u totals).\n",
             nMatchedLines,
             nMatchedBlocks,
             nTotalMatchedLines,
             nTotalMatchedBlocks
            );
    fflush (stderr);
    if (3 < opt_d_Debug) {
      DumpFilesLines ();
    }
  }
}

// Perform Pass #3, in which we attempt to 'widen' blocks of matched
// lines, by prepending lines which are the same in both files.

void pass3 ()
{
  if (0 < opt_d_Debug) {
    fprintf (stderr, "# Pass #3 (spreading matches up)...");
    fflush (stderr);
  }

  nMatchedBlocks = 0;
  nMatchedLines = 0;

  // Starting at the bottom of the old file,...

  for (int o = lines[0].size () - 1; 0 <= o; o -= 1) {

    if (2 < opt_d_Debug) {
      DumpFileLine (0, o);
    }

    // ... skip any currently unmatched lines...

    if (lines[0][o].l == ~0u) {
      continue;
    }

    // ... until we find an [already] matched line.  Then, skip over
    // the set of 1 or more matched lines, until we reach any
    // following, still-unmatched line.

    unsigned n;
    do {
      n = lines[0][o].l - 1;
      o -= 1;

      if (2 < opt_d_Debug) {
        DumpFileLine (0, o);
        DumpFileLine (1, n);
      }
    } while (0 <= o && lines[0][o].l != ~0u);

    // Now, o + 1 is the line number of the last matched old line, and
    // n + 1 is the line number of the last matched new line.  We want
    // to add new old and new lines, if they match.

    // Finally, attempt to add unmatched lines to the preceeding
    // matched set.

    for (; 0 <= o && lines[0][o].l == ~0u; o -= 1, n -= 1) {

      // If the [next] pair of old and new lines aren't the same line,
      // we're done.

      if (lines[0][o].line != lines[1][n].line) {
        break;
      }

      // Match up the pair.

      lines[0][o].l = n;
      lines[1][n].l = o;

      // Remove the matched line numbers form the lists.

      Line *line = lines[0][o].line;
      list<unsigned> &oldCopies = line->copies[0];
      list<unsigned>::iterator oL =
        find (oldCopies.begin (), oldCopies.end (), o);
      if (oL != oldCopies.end ()) {
        oldCopies.erase (oL);
      }
      list<unsigned> &newCopies = line->copies[1];
      list<unsigned>::iterator nL =
        find (newCopies.begin (), newCopies.end (), n);
      if (nL != newCopies.end ()) {
        newCopies.erase (nL);
      }

      if (2 < opt_d_Debug) {
        DumpFileLine (0, o);
        DumpFileLine (1, n);
      }
      nMatchedLines += 1;
    }
    nMatchedBlocks += 1;
  }
  nTotalMatchedLines += nMatchedLines;

  // Let's see the intermediate results.

  if (0 < opt_d_Debug) {
    fprintf (stderr,
             " found %u/%u matched lines/blocks (%u/%u totals).\n",
             nMatchedLines,
             nMatchedBlocks,
             nTotalMatchedLines,
             nTotalMatchedBlocks
            );
    fflush (stderr);

    if (3 < opt_d_Debug) {
      DumpFilesLines ();
    }
  }
}

// Perform pass #4, in which we find any remaining matches amongst the
// unique lines.  For every unique line that has more than 1 copy in
// both files, we declare the copies to match.

void pass4 ()
{
  if (0 < opt_d_Debug) {
    fprintf (stderr,
             "# Pass 4 (finding remaining matches amongst unique lines)..."
            );
    fflush (stderr);
  }

  nMatchedLines = 0;

  for (MapStringToLinePtr::iterator i = table.begin ();
       i != table.end ();
       i++
      )
  {
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

      if (2 < opt_d_Debug) {
        DumpFileLine (0, o);
        DumpFileLine (1, n);
      }
    }
  }
  nTotalMatchedLines += nMatchedLines;

  // Let's see the intermediate results.

  if (0 < opt_d_Debug) {
    fprintf (stderr,
             " found %u matched lines (%u total).\n",
             nMatchedLines,
             nTotalMatchedLines
            );
    fflush (stderr);

    if (3 < opt_d_Debug) {
      DumpFilesLines ();
    }
  }
}

// Perform Pass #5, in which we look for blocks which match, but are
// the result of a move (their not in their original position).  For
// these, we'll unmatch the blocks, turning them into a delete and an
// insert.

void pass5 ()
{
  if (0 < opt_d_Debug) {
    fprintf (stderr, "# Pass #5 (unmatching block moves)...\n");
    fflush (stderr);
  }

  nMatchedBlocks = 0;
  nMatchedLines = 0;

  // Starting at the top of both files,...

  unsigned o = 0;
  unsigned n = 0;

  while (o < lines[0].size () || n < lines[1].size ()) {

    if (2 < opt_d_Debug) {
      DumpFileLine (0, o);
      DumpFileLine (1, n);
    }

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

    while (o < lines[0].size () && lines[0][o].l == ~0u) {
      o += 1;

      if (2 < opt_d_Debug) {
        DumpFileLine (0, o);
      }
    }

    // Skip any unmatched lines at this point in the new file.  These
    // are inserts.

    while (n < lines[1].size () && lines[1][n].l == ~0u) {
      n += 1;

      if (2 < opt_d_Debug) {
        DumpFileLine (1, n);
      }
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

      if (2 < opt_d_Debug) {
        DumpFileLine (0, o);
        DumpFileLine (1, n);
      }
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

    for (n = nNew;
         o < lines[0].size () && n < lines[1].size ();
         o += 1, n += 1
        )
    {
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
        lines[0][a].l = ~0u;

        if (2 < opt_d_Debug) {
          DumpFileLine (0, o);
        }
      }
      for (unsigned d = nNew; d < n; d += 1) {
        lines[1][d].l = ~0u;

        if (2 < opt_d_Debug) {
          DumpFileLine (1, n);
        }
      }
    // } else {
      // for (unsigned a = oOld; a < o; a += 1) {
      //   lines[0][a].l = ~0u;
      // }
      // for (unsigned d = nNew; d < n; d += 1) {
      //   lines[1][d].l = ~0u;
      // }
    // }

    // Continue from where we left off.

    n = nOld;
  }
  nTotalMatchedLines -= nMatchedLines;
  nTotalMatchedBlocks -= nMatchedBlocks;

  // Let's see the intermediate results.

  if (0 < opt_d_Debug) {
    fprintf (stderr,
             " found %u/%u matched lines/blocks (%u/%u totals).\n",
             nMatchedLines,
             nMatchedBlocks,
             nTotalMatchedLines,
             nTotalMatchedBlocks
            );
    fflush (stderr);

    if (3 < opt_d_Debug) {
      DumpFilesLines ();
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
  switch (opt___OutputFormat) {
  case opt_c_Context:
    pass6c ();
    break;

  case opt_e_EdScript:
    pass6e ();
    break;

  case opt_D_IfThenElse:
    pass6i ();
    break;

  case opt___Normal:
    pass6n ();
    break;

  case opt_n_RCS:
    pass6r ();
    break;

  case opt_y_SideBySide:
    pass6y ();
    break;

  case opt_u_Unified:
    pass6u ();
    break;

  default:
    pass6n ();
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
  if (0 < opt_d_Debug) {
    fprintf (stderr, "# Pass #6c (walking the differences)...\n");
    fflush (stderr);
  }

  // Write the header.

  fprintf (stdout,
           "*** %s\t%s\n",
           files[0],
           "0000-00-00 00:00:00.000000000 +0000"
          );
  fprintf (stdout,
           "--- %s\t%s\n",
           files[1],
           "0000-00-00 00:00:00.000000000 +0000"
          );

  // Starting at the top of both files,...

  unsigned o = 1;
  unsigned oEoF = lines[0].size () - 1;
  unsigned n = 1;
  unsigned nEoF = lines[1].size () - 1;

  while (o < oEoF || n < nEoF) {

    if (2 < opt_d_Debug) {
      DumpFileLine (0, o);
      DumpFileLine (1, n);
    }

    // Find the bounds of the current contextual 'window'.

    // If we aren't looking at a delete or an insert, we're not at the
    // start of a context window.

    if (lines[0][o].l != ~0u && lines[1][n].l != ~0u) {
      assert (lines[0][o].line == lines[1][n].line);
      o += 1;
      n += 1;
      continue;
    }

    // We're now looking at at least 1 delete or insert, so a window
    // begins here (or, rather, opt_C_LinesOfCopyContext lines earlier).
    
    int boOldWindow = max (1, int (o) - int (opt_C_LinesOfCopyContext));
    int boNewWindow = max (1, int (n) - int (opt_C_LinesOfCopyContext));

    if (1 < opt_d_Debug) {
      fprintf (stderr,
               "# boOldWindow = %d, boNewWindow = %d\n",
               boOldWindow,
               boNewWindow
              );
      fflush (stderr);
    }

    // Look for the end of the current window.

    int eoOldWindow;
    int eoNewWindow;

    bool widenWindow = true;
    while (widenWindow && o < oEoF || n < nEoF) {

      // Skip over the delete(s) and/or insert(s) defining the current
      // context.

      while (o < oEoF && lines[0][o].l == ~0u) {
        o += 1;

        if (2 < opt_d_Debug) {
          DumpFileLine (0, o);
        }
      }
      while (n < nEoF && lines[1][n].l == ~0u) {
        n += 1;

        if (2 < opt_d_Debug) {
          DumpFileLine (1, n);
        }
      }

      // This context will end opt_C_LinesOfCopyContext matched lines
      // past the last set of deletes or inserts we find.

      widenWindow = false;
      for (unsigned l = 0;
           !widenWindow && l < opt_C_LinesOfCopyContext;
           l += 1
          )
      {
        if (o < oEoF) {
          widenWindow |= lines[0][o].l == ~0u;
          o += 1;

          if (2 < opt_d_Debug) {
            DumpFileLine (0, o);
          }
        }
        if (n < nEoF) {
          widenWindow |= lines[1][n].l == ~0u;
          n += 1;

          if (2 < opt_d_Debug) {
            DumpFileLine (1, n);
          }
        }

        // fprintf (stderr,
        //          "# widenWindow = %s",
        //          widenWindow ? "true" : "false"
        //         );
        // if (!widenWindow) {
        //   fprintf (stderr,
        //            " || lines[0][%d].line (%p) %s lines[1][%d].line (%p)",
        //            o,
        //            lines[0][o].line,
        //            lines[0][o].line == lines[1][n].line ? "==" : "!=",
        //            n,
        //            lines[1][n].line
        //           );
        // }
        // fprintf (stderr, "\n");
        // 
        // assert (widenWindow || lines[0][o].line == lines[1][n].line);
      }

      eoOldWindow = o;
      eoNewWindow = n;

      // However, since another window could follow, we look
      // opt_C_LinesOfCopyContext ahead for another delete or insert.

      if (!widenWindow) {
        for (unsigned l = 0; l < (2 * opt_C_LinesOfCopyContext + 1); l += 1) {
          if (o < oEoF) {
            widenWindow |= lines[0][o].l == ~0u;
            o += 1;

            if (2 < opt_d_Debug) {
              DumpFileLine (0, o);
            }
          }
          if (n < nEoF) {
            widenWindow |= lines[1][n].l == ~0u;
            n += 1;

            if (2 < opt_d_Debug) {
              DumpFileLine (1, n);
            }
          }

          // fprintf (stderr,
          //          "# widenWindow = %s",
          //          widenWindow ? "true" : "false"
          //         );
          // if (!widenWindow) {
          //   fprintf (stderr,
          //            " || lines[0][%d].line (%p) %s lines[1][%d].line (%p)",
          //            o,
          //            lines[0][o].line,
          //            lines[0][o].line == lines[1][n].line ? "==" : "!=",
          //            n,
          //            lines[1][n].line
          //           );
          // }
          // fprintf (stderr, "\n");
          // 
          // assert (widenWindow || lines[0][o].line == lines[1][n].line);
        }
      }
    }

    // We've found the end of the window.

    if (1 < opt_d_Debug) {
      fprintf (stderr,
               "eoOldWindow = %d, eoNewWindow = %d\n",
               eoOldWindow,
               eoNewWindow
              );
      fflush (stderr);
    }

    // Now, reset ourselves to the start of the window, and walk it to
    // produce the output.  We'll walk it twice, first for the old
    // file part, then for the new file part.

    fprintf (stdout, "***************\n");
    fprintf (stdout, "*** %d,%d ****\n", boOldWindow, eoOldWindow - 1);

    o = boOldWindow;
    n = boNewWindow;

    while (o < eoOldWindow || n < eoNewWindow) {

      // Find any unmatched lines at this point in the old file.  These
      // are deletes.

      unsigned boDeletes = o;
      while (o < eoOldWindow && lines[0][o].l == ~0u) {
        o += 1;
      }

      // Find any unmatched lines at this point in the new file.  These
      // are inserts.

      unsigned boInserts = n;
      while (n < eoNewWindow && lines[1][n].l == ~0u) {
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

      assert (lines[0][o].line == lines[1][n].line);

      fprintf (stdout, "  %s\n", lines[0][o].line->c_str ());
      o += 1;
      n += 1;
    }

    fprintf (stdout, "--- %d,%d ----\n", boNewWindow, eoNewWindow - 1);

    o = boOldWindow;
    n = boNewWindow;

    while (o < eoOldWindow && n < eoNewWindow) {

      // Find any unmatched lines at this point in the old file.  These
      // are deletes.

      unsigned boDeletes = o;
      while (o < eoOldWindow && lines[0][o].l == ~0u) {
        o += 1;
      }

      // Find any unmatched lines at this point in the new file.  These
      // are inserts.

      unsigned boInserts = n;
      while (n < eoNewWindow && lines[1][n].l == ~0u) {
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

      assert (lines[0][o].line == lines[1][n].line);

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
  if (0 < opt_d_Debug) {
    fprintf (stderr, "# Pass #6n (walking the differences)...\n");
    fflush (stderr);
  }

  // Write the header.  (None for normal.)

  // Starting at the top of both files,...

  unsigned o = 1;
  unsigned oEoF = lines[0].size () - 1;
  unsigned n = 1;
  unsigned nEoF = lines[1].size () - 1;

  while (o < oEoF || n < nEoF) {

    if (2 < opt_d_Debug) {
      DumpFileLine (0, o);
      DumpFileLine (1, n);
    }

    // Find any unmatched lines at this point in the old file.  These
    // are deletes.

    unsigned boDeletes = o;
    while (o < oEoF && lines[0][o].l == ~0u) {
      o += 1;

      if (2 < opt_d_Debug) {
        DumpFileLine (0, o);
      }
    }

    // Find any unmatched lines at this point in the new file.  These
    // are inserts.

    unsigned boInserts = n;
    while (n < nEoF && lines[1][n].l == ~0u) {
      n += 1;

      if (2 < opt_d_Debug) {
        DumpFileLine (1, n);
      }
    }

    // We've got deleted line(s) from boDeletes .. o.
    // We've got deleted line(s) from boInserts .. n.

    unsigned nDeletes = o - boDeletes;
    unsigned nInserts = n - boInserts;

    // We've got...

    if (nDeletes && nInserts) {

      // ... deletes and inserts.

      if (1 < nDeletes && 1 < nInserts) {
        fprintf (stdout, "%d,%dc%d,%d\n", boDeletes, o - 1, boInserts, n - 1);
      } else if (1 < nDeletes) {
        fprintf (stdout, "%d,%dc%d\n", boDeletes, o - 1, boInserts);
      } else if (1 < nInserts) {
        fprintf (stdout, "%dc%d,%d\n", boDeletes, boInserts, n - 1);
      } else {
        fprintf (stdout, "%dc%d\n", boDeletes, boInserts);
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
        fprintf (stdout, "%d,%dd%d\n", boDeletes, o - 1, n - 1);
      } else {
        fprintf (stdout, "%dd%d\n", boDeletes, n - 1);
      }
      for (unsigned l = boDeletes; l < o; l += 1) {
        fprintf (stdout, "< %s\n", lines[0][l].line->c_str ());
      }
    } else if (nInserts) {

      //  ... just inserts.

      if (1 < nInserts) {
        fprintf (stdout, "%da%d,%d\n", o - 1, boInserts, n - 1);
      } else {
        fprintf (stdout, "%da%d\n", o - 1, boInserts);
      }
      for (unsigned l = boInserts; l < n; l += 1) {
        fprintf (stdout, "> %s\n", lines[1][l].line->c_str ());
      }
    }

    // When we get here, we're dealing with matching lines.

    assert (lines[0][o].line == lines[1][n].line);

    if (o < oEoF) {
      o += 1;
    }
    if (n < nEoF) {
      n += 1;
    }
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
  if (0 < opt_d_Debug) {
    fprintf (stderr, "# Pass #6u (walking the differences)...\n");
    fflush (stderr);
  }

  // Write the header.

  fprintf (stdout,
           "--- %s\t%s\n",
           files[0],
           "0000-00-00 00:00:00.000000000 +0000"
          );
  fprintf (stdout,
           "+++ %s\t%s\n",
           files[1],
           "0000-00-00 00:00:00.000000000 +0000"
          );
  fprintf (stdout,
           "@@ -1,%d +1,%d @@\n",
           lines[0].size () - 2,
           lines[1].size () - 2
          );

  // Starting at the top of both files,...

  unsigned o = 1;
  unsigned oEoF = lines[0].size () - 1;
  unsigned n = 1;
  unsigned nEoF = lines[1].size () - 1;

  while (o < oEoF || n < nEoF) {

    if (2 < opt_d_Debug) {
      DumpFileLine (0, o);
      DumpFileLine (1, n);
    }

    // Find any unmatched lines at this point in the old file.  These
    // are deletes.

    unsigned boDeletes = o;
    while (o < oEoF && lines[0][o].l == ~0u) {
      o += 1;

      if (2 < opt_d_Debug) {
        DumpFileLine (0, o);
      }
    }

    // Find any unmatched lines at this point in the new file.  These
    // are inserts.

    unsigned boInserts = n;
    while (n < nEoF && lines[1][n].l == ~0u) {
      n += 1;

      if (2 < opt_d_Debug) {
        DumpFileLine (1, n);
      }
    }

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

    assert (lines[0][o].line == lines[1][n].line);

    fprintf (stdout, " %s\n", lines[0][o].line->c_str ());

    if (o < oEoF) {
      o += 1;
    }
    if (n < nEoF) {
      n += 1;
    }
  }
}

void pass6y ()
{
  if (0 < opt_d_Debug) {
    fprintf (stderr, "# Pass #6y (walking the differences)...\n");
    fflush (stderr);
  }

  // Write the header.

  // None for side-by-side.

  // Starting at the top of both files,...

  int columnWidth = (opt_W_MaxPrintColumns - 7) / 2;

  unsigned o = 1;
  unsigned oEoF = lines[0].size () - 1;
  unsigned n = 1;
  unsigned nEoF = lines[1].size () - 1;

  while (o < oEoF || n < nEoF) {

    if (2 < opt_d_Debug) {
      DumpFileLine (0, o);
      DumpFileLine (1, n);
    }

    // Find any unmatched lines at this point in the old file.  These
    // are deletes.

    unsigned boDeletes = o;
    while (o < oEoF && lines[0][o].l == ~0u) {
      o += 1;

      if (2 < opt_d_Debug) {
        DumpFileLine (0, o);
      }
    }

    // Find any unmatched lines at this point in the new file.  These
    // are inserts.

    unsigned boInserts = n;
    while (n < nEoF && lines[1][n].l == ~0u) {
      n += 1;

      if (2 < opt_d_Debug) {
        DumpFileLine (1, n);
      }
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

    assert (lines[0][o].line == lines[1][n].line);

    fprintf (stdout,
             "%-*s   %s\n",
             columnWidth,
             lines[0][o].line->substr (0, columnWidth).c_str (),
             lines[1][n].line->substr (0, columnWidth).c_str ()
            );

    if (o < oEoF) {
      o += 1;
    }
    if (n < nEoF) {
      n += 1;
    }
  }
}
