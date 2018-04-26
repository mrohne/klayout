
/*

  KLayout Layout Viewer
  Copyright (C) 2006-2017 Matthias Koefferlein

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/



#include "dbCIFReader.h"
#include "dbStream.h"
#include "dbObjectWithProperties.h"
#include "dbArray.h"
#include "dbStatic.h"

#include "tlException.h"
#include "tlString.h"
#include "tlClassRegistry.h"

#include <cctype>

namespace db
{

// ---------------------------------------------------------------
//  CIFReader


CIFReader::CIFReader (tl::InputStream &s)
  : m_stream (s),
    m_create_layers (true),
    m_progress (tl::to_string (QObject::tr ("Reading CIF file")), 1000),
    m_dbu (0.001), m_wire_mode (0), m_next_layer_index (0)
{
  m_progress.set_format (tl::to_string (QObject::tr ("%.0fk lines")));
  m_progress.set_format_unit (1000.0);
  m_progress.set_unit (100000.0);
}

CIFReader::~CIFReader ()
{
  //  .. nothing yet ..
}

const LayerMap &
CIFReader::read (db::Layout &layout, const db::LoadLayoutOptions &options)
{
  m_dbu = 0.001;
  m_wire_mode = 0;
  m_next_layer_index = 0;

  const db::CIFReaderOptions &specific_options = options.get_options<db::CIFReaderOptions> ();
  m_wire_mode = specific_options.wire_mode;
  m_dbu = specific_options.dbu;

  m_layer_map = specific_options.layer_map;
  m_layer_map.prepare (layout);
  m_create_layers = specific_options.create_other_layers;
  do_read (layout);
  return m_layer_map;
}

const LayerMap &
CIFReader::read (db::Layout &layout)
{
  return read (layout, db::LoadLayoutOptions ());
}

void 
CIFReader::error (const std::string &msg)
{
  throw CIFReaderException (msg, m_stream.line_number (), m_cellname);
}

void 
CIFReader::warn (const std::string &msg) 
{
  // TODO: compress
  tl::warn << msg 
           << tl::to_string (QObject::tr (" (line=")) << m_stream.line_number ()
           << tl::to_string (QObject::tr (", cell=")) << m_cellname
           << ")";
}

/**
 *  @brief Skip blanks in the sense of CIF
 *  A blank in CIF is "any ASCII character except digit, upperChar, '-', '(', ')', or ';'"
 */
void 
CIFReader::skip_blanks()
{
  while (! m_stream.at_end ()) {
    char c = m_stream.peek_char ();
    if (isupper (c) || isdigit (c) || c == '-' || c == '(' || c == ')' || c == ';') {
      return;
    }
    m_stream.get_char ();
  }
}

/**
 *  @brief Skips separators
 */
void 
CIFReader::skip_sep ()
{
  while (! m_stream.at_end ()) {
    char c = m_stream.peek_char ();
    if (isdigit (c) || c == '-' || c == '(' || c == ')' || c == ';') {
      return;
    }
    m_stream.get_char ();
  }
}

/**
 *  @brief Skip comments
 *  This assumes that the reader is after the first '(' and it will stop
 *  after the final ')'.
 */
void 
CIFReader::skip_comment ()
{
  char c;
  int bl = 0;
  while (! m_stream.at_end () && ((c = m_stream.get_char ()) != ')' || bl > 0)) {
    // check for nested comments (bl is the nesting level)
    if (c == '(') {
      ++bl;
    } else if (c == ')') {
      --bl;
    }
  }
}

/**
 *  @brief Gets a character and issues an error if the stream is at the end
 */
char 
CIFReader::get_char ()
{
  if (m_stream.at_end ()) {
    error ("Unexpected end of file");
    return 0;
  } else {
    return m_stream.get_char ();
  }
}

/**
 *  @brief Tests whether the next character is a semicolon (after blanks)
 */
bool 
CIFReader::test_semi ()
{
  skip_blanks ();
  if (! m_stream.at_end () && m_stream.peek_char () == ';') {
    return true;
  } else {
    return false;
  }
}

/**
 *  @brief Tests whether a semicolon follows and issue an error if not
 */
void
CIFReader::expect_semi ()
{
  if (! test_semi ()) {
    error ("Expected ';' command terminator");
  } else {
    get_char ();
  }
}

/**
 *  @brief Skips all until the next semicolon
 */
void
CIFReader::skip_to_end ()
{
  while (! m_stream.at_end () && m_stream.get_char () != ';') {
    ;
  }
}

/**
 *  @brief Fetches an integer
 */
int 
CIFReader::read_integer_digits ()
{
  if (m_stream.at_end () || ! isdigit (m_stream.peek_char ())) {
    error ("Digit expected");
  }

  int i = 0;
  while (! m_stream.at_end () && isdigit (m_stream.peek_char ())) {

    if (i > std::numeric_limits<int>::max () / 10) {

      error ("Integer overflow");
      while (! m_stream.at_end () && isdigit (m_stream.peek_char ())) {
        m_stream.get_char ();
      }

      return 0;

    }

    char c = m_stream.get_char ();
    i = i * 10 + int (c - '0');

  }

  return i;
}

/**
 *  @brief Fetches an integer
 */
int 
CIFReader::read_integer ()
{
  skip_sep ();
  return read_integer_digits ();
}

/**
 *  @brief Fetches a signed integer
 */
int 
CIFReader::read_sinteger ()
{
  skip_sep ();

  bool neg = false;
  if (m_stream.peek_char () == '-') {
    m_stream.get_char ();
    neg = true;
  }

  int i = read_integer_digits ();
  return neg ? -i : i;
}

/**
 *  @brief Fetches a string (layer name)
 */
const std::string &
CIFReader::read_name ()
{
  skip_blanks ();

  m_cmd_buffer.clear ();
  if (m_stream.at_end ()) {
    return m_cmd_buffer;
  }

  //  Note: officially only upper and digits are allowed in names. But we allow lower case and "_" too ...
  while (! m_stream.at_end () && (isupper (m_stream.peek_char ()) || islower (m_stream.peek_char ()) || m_stream.peek_char () == '_' || isdigit (m_stream.peek_char ()))) {
    m_cmd_buffer += m_stream.get_char ();
  }

  return m_cmd_buffer;
}

/**
 *  @brief Fetches a string (in labels, texts)
 */
const std::string &
CIFReader::read_string ()
{
  m_stream.skip ();

  m_cmd_buffer.clear ();
  if (m_stream.at_end ()) {
    return m_cmd_buffer;
  }

  char q = m_stream.peek_char ();
  if (q == '"' || q == '\'') {

    get_char ();

    //  read a quoted string (KLayout extension)
    while (! m_stream.at_end () && m_stream.peek_char () != q) {
      char c = m_stream.get_char ();
      if (c == '\\' && ! m_stream.at_end ()) {
        c = m_stream.get_char ();
      }
      m_cmd_buffer += c;
    }

    if (! m_stream.at_end ()) {
      get_char ();
    }

  } else {

    while (! m_stream.at_end () && !isspace (m_stream.peek_char ()) && m_stream.peek_char () != ';') {
      m_cmd_buffer += m_stream.get_char ();
    }

  }

  return m_cmd_buffer;
}

/**
 *  @brief Reads a double value (extension)
 */
double
CIFReader::read_double ()
{
  m_stream.skip ();

  //  read a quoted string (KLayout extension)
  m_cmd_buffer.clear ();
  while (! m_stream.at_end () && (isdigit (m_stream.peek_char ()) || m_stream.peek_char () == '.' || m_stream.peek_char () == '-' || m_stream.peek_char () == 'e' || m_stream.peek_char () == 'E')) {
    m_cmd_buffer += m_stream.get_char ();
  }

  double v = 0.0;
  try {tl::from_string (m_cmd_buffer, v);}
  catch (...) { }
  return v;
}

static bool 
extract_plain_layer (const char *s, int &l)
{
  l = 0;
  if (! *s) {
    return false;
  }
  while (*s && isdigit (*s)) {
    l = l * 10 + (unsigned int) (*s - '0');
    ++s;
  }
  return (*s == 0);
}

static bool 
extract_ld (const char *s, int &l, int &d, std::string &n)
{
  l = d = 0;

  if (*s == 'L') {
    ++s;
  }

  if (! *s || ! isdigit (*s)) {
    return false;
  }

  while (*s && isdigit (*s)) {
    l = l * 10 + (unsigned int) (*s - '0');
    ++s;
  }

  if (*s == 'D' || *s == '.') {
    ++s;
    if (! *s || ! isdigit (*s)) {
      return false;
    }
    while (*s && isdigit (*s)) {
      d = d * 10 + (unsigned int) (*s - '0');
      ++s;
    }
  }

  if (*s && (isspace (*s) || *s == '_')) {
    ++s;
    n = s;
    return true;
  } else if (*s == 0) {
    n.clear ();
    return true;
  } else {
    return false;
  }
}

bool
CIFReader::read_cell (db::Layout &layout, db::Cell &cell, double sf, int level)
{
  if (fabs (sf - floor (sf + 0.5)) > 1e-6) {
    warn ("Scaling factor is not an integer - snapping errors may occur in cell '" + m_cellname + "'");
  }

  int nx = 0, ny = 0, dx = 0, dy = 0;
  int layer = -2; // no layer defined yet.
  int path_mode = -1;
  size_t insts = 0;
  size_t shapes = 0;
  size_t layer_specs = 0;
  std::vector <db::Point> poly_pts;

  while (true) {

    skip_blanks ();

    char c = get_char ();
    if (c == ';') {

      //  empty command

    } else if (c == '(') {

      skip_comment ();

    } else if (c == 'E') {

      if (level > 0) {
        error ("'E' command must be outside a cell specification");
      } 

      skip_blanks ();
      break;

    } else if (c == 'D') {

      skip_blanks ();

      c = get_char ();
      if (c == 'S') {

        //  DS command:
        //  "D" blank* "S" integer (sep integer sep integer)?

        unsigned int n = 0, denom = 1, divider = 1;
        n = read_integer ();
        if (! test_semi ()) {
          denom = read_integer ();
          divider = read_integer ();
        }

        expect_semi ();

        std::string outer_cell = "C" + tl::to_string (n);
        std::swap (m_cellname, outer_cell);

        std::map <unsigned int, db::cell_index_type>::const_iterator c = m_cells_by_id.find (n);
        db::cell_index_type ci;
        if (c == m_cells_by_id.end ()) {
          ci = layout.add_cell (m_cellname.c_str ());
          m_cells_by_id.insert (std::make_pair (n, ci));
        } else {
          ci = c->second;
        } 

        db::Cell &cell = layout.cell (ci);

        read_cell (layout, cell, sf * double (denom) / double (divider), level + 1);

        std::swap (m_cellname, outer_cell);

      } else if (c == 'F') {

        // DF command:
        // "D" blank* "F"
        if (level == 0) {
          error ("'DS' command must be inside a cell specification");
        } 

        //  skip the rest of the command
        skip_to_end ();

        break;

      } else if (c == 'D') {

        //  DD command:
        //  "D" blank* "D" integer

        read_integer ();
        warn ("DD command ignored");
        skip_to_end ();

      } else {

        error ("Invalid 'D' sub-command");
        skip_to_end ();

      }

    } else if (c == 'C') {

      //  C command:
      //  "C" integer transformation
      //  transformation := (blank* ("T" point |"M" blank* "X" |"M" blank* "Y" |"R" point)*)*

      ++insts;

      unsigned int n = read_integer ();
      std::map <unsigned int, db::cell_index_type>::const_iterator c = m_cells_by_id.find (n);
      if (c == m_cells_by_id.end ()) {
        std::string cn = "C" + tl::to_string (n);
        c = m_cells_by_id.insert (std::make_pair (n, layout.add_cell (cn.c_str ()))).first;
      } 

      db::DCplxTrans trans;

      while (! test_semi ()) {

        skip_blanks ();

        char ct = get_char ();
        if (ct == 'M') {

          skip_blanks ();

          char ct2 = get_char ();
          if (ct2 == 'X') {
            trans = db::DCplxTrans (db::FTrans::m90) * trans;
          } else if (ct2 == 'Y') {
            trans = db::DCplxTrans (db::FTrans::m0) * trans;
          } else {
            error ("Invalid 'M' transformation specification");
            //  skip everything to avoid more errors here
            while (! test_semi ()) {
              get_char ();
            }
          }

        } else if (ct == 'T') {

          int x = read_sinteger ();
          int y = read_sinteger ();
          trans = db::DCplxTrans (db::DVector (x * sf, y * sf)) * trans;

        } else if (ct == 'R') {

          int x = read_sinteger ();
          int y = read_sinteger ();

          if (y != 0 || x != 0) {
            double a = atan2 (double (y), double (x)) * 180.0 / M_PI;
            trans = db::DCplxTrans (1.0, a, false, db::DVector ()) * trans;
          }

        } else {
          error ("Invalid transformation specification");
          //  skip everything to avoid more errors here
          while (! test_semi ()) {
            get_char ();
          }
        }

      }

      if (nx > 0 || ny > 0) {
        if (trans.is_ortho () && ! trans.is_mag ()) {
          cell.insert (db::CellInstArray (db::CellInst (c->second), db::Trans (db::ICplxTrans (trans)), db::Vector (dx * sf, 0.0), db::Vector (0.0, dy * sf), std::max (1, nx), std::max (1, ny)));
        } else {
          cell.insert (db::CellInstArray (db::CellInst (c->second), db::ICplxTrans (trans), db::Vector (dx * sf, 0.0), db::Vector (0.0, dy * sf), std::max (1, nx), std::max (1, ny)));
        }
      } else {
        if (trans.is_ortho () && ! trans.is_mag ()) {
          cell.insert (db::CellInstArray (db::CellInst (c->second), db::Trans (db::ICplxTrans (trans))));
        } else {
          cell.insert (db::CellInstArray (db::CellInst (c->second), db::ICplxTrans (trans)));
        }
      }

      nx = ny = 0;
      dx = dy = 0;

      expect_semi ();

    } else if (c == 'L') {

      skip_blanks ();

      ++layer_specs;

      std::string name = read_name ();
      if (name.empty ()) {
        error ("Missing layer name in 'L' command");
      }

      std::pair<bool, unsigned int> ll = m_layer_map.logical (name);
      if (! ll.first) {

        int l = -1, d = -1;
        std::string on;

        if (extract_plain_layer (name.c_str (), l)) {

          db::LayerProperties lp;
          lp.layer = l;
          lp.datatype = 0;
          ll = m_layer_map.logical (lp);

        } else if (extract_ld (name.c_str (), l, d, on)) {

          db::LayerProperties lp;
          lp.layer = l;
          lp.datatype = d;
          lp.name = on;
          ll = m_layer_map.logical (lp);

        }

      }

      if (ll.first) {

        //  create the layer if it is not part of the layout yet.
        if (! layout.is_valid_layer (ll.second)) {
          layout.insert_layer (ll.second, m_layer_map.mapping (ll.second));
        }

        layer = int (ll.second);

      } else if (! m_create_layers) {

        layer = -1; // ignore geometric objects on this layer

      } else {

        std::map <std::string, unsigned int>::const_iterator nl = m_new_layers.find (name);
        if (nl == m_new_layers.end ()) {

          unsigned int ll = m_next_layer_index++;

          layout.insert_layer (ll, db::LayerProperties ());
          m_new_layers.insert (std::make_pair (name, ll));

          layer = int (ll);

        } else {
          layer = int (nl->second);
        }

      }

      expect_semi ();

    } else if (c == 'B') {

      ++shapes;

      if (layer < 0) {

        if (layer < -1) {
          warn ("'B' command ignored since no layer was selected");
        }
        skip_to_end ();

      } else {

        unsigned int w = 0, h = 0;
        int x = 0, y = 0;

        w = read_integer ();
        h = read_integer ();
        x = read_sinteger ();
        y = read_sinteger ();

        int rx = 0, ry = 0;
        if (! test_semi ()) {
          rx = read_sinteger ();
          ry = read_sinteger ();
        }

        if (rx >= 0 && ry == 0) {

          cell.shapes ((unsigned int) layer).insert (db::Box (sf * (x - 0.5 * w), sf * (y - 0.5 * h), sf * (x + 0.5 * w), sf * (y + 0.5 * h)));

        } else {

          double n = 1.0 / sqrt (double (rx) * double (rx) + double (ry) * double (ry));

          double xw = sf * w * 0.5 * rx * n, yw = sf * w * 0.5 * ry * n;
          double xh = -sf * h * 0.5 * ry * n, yh = sf * h * 0.5 * rx * n;

          db::Point points [4];
          points [0] = db::Point (x - xw - xh, y - yw - yh);
          points [1] = db::Point (x - xw + xh, y - yw + yh);
          points [2] = db::Point (x + xw + xh, y + yw + yh);
          points [3] = db::Point (x + xw - xh, y + yw - yh);

          db::Polygon p;
          p.assign_hull (points, points + 4);
          cell.shapes ((unsigned int) layer).insert (p);

        }

        expect_semi ();

      }

    } else if (c == 'P') {

      ++shapes;

      if (layer < 0) {

        if (layer < -1) {
          warn ("'P' command ignored since no layer was selected");
        }
        skip_to_end ();

      } else {

        poly_pts.clear ();

        while (! test_semi ()) {

          int rx = read_sinteger ();
          int ry = read_sinteger ();

          poly_pts.push_back (db::Point (sf * rx, sf * ry));

        }

        db::Polygon p;
        p.assign_hull (poly_pts.begin (), poly_pts.end ());
        cell.shapes ((unsigned int) layer).insert (p);

        expect_semi ();

      }

    } else if (c == 'R') {

      ++shapes;

      if (layer < 0) {

        if (layer < -1) {
          warn ("'R' command ignored since no layer was selected");
        }
        skip_to_end ();

      } else {

        int w = read_integer ();

        poly_pts.clear ();

        int rx = read_sinteger ();
        int ry = read_sinteger ();

        poly_pts.push_back (db::Point (sf * rx, sf * ry));

        db::Path p (poly_pts.begin (), poly_pts.end (), db::coord_traits <db::Coord>::rounded (sf * w), db::coord_traits <db::Coord>::rounded (sf * w / 2), db::coord_traits <db::Coord>::rounded (sf * w / 2), true);
        cell.shapes ((unsigned int) layer).insert (p);

        expect_semi ();

      }

    } else if (c == 'W') {

      ++shapes;

      if (layer < 0) {

        if (layer < -1) {
          warn ("'W' command ignored since no layer was selected");
        }

        skip_to_end ();

      } else {

        int w = read_integer ();

        poly_pts.clear ();

        while (! test_semi ()) {

          int rx = read_sinteger ();
          int ry = read_sinteger ();

          poly_pts.push_back (db::Point (sf * rx, sf * ry));

        }

        if (path_mode == 0 || (path_mode < 0 && m_wire_mode == 1)) {
          //  Flush-ended paths 
          db::Path p (poly_pts.begin (), poly_pts.end (), db::coord_traits <db::Coord>::rounded (sf * w), 0, 0, false);
          cell.shapes ((unsigned int) layer).insert (p);
        } else if (path_mode == 1 || (path_mode < 0 && m_wire_mode == 2)) {
          //  Round-ended paths
          db::Path p (poly_pts.begin (), poly_pts.end (), db::coord_traits <db::Coord>::rounded (sf * w), db::coord_traits <db::Coord>::rounded (sf * w / 2), db::coord_traits <db::Coord>::rounded (sf * w / 2), true);
          cell.shapes ((unsigned int) layer).insert (p);
        } else {
          //  Square-ended paths
          db::Path p (poly_pts.begin (), poly_pts.end (), db::coord_traits <db::Coord>::rounded (sf * w), db::coord_traits <db::Coord>::rounded (sf * w / 2), db::coord_traits <db::Coord>::rounded (sf * w / 2), false);
          cell.shapes ((unsigned int) layer).insert (p);
        }

        expect_semi ();

      }

    } else if (isdigit (c)) {

      char cc = m_stream.peek_char ();
      if (c == '9' && cc == '3') {

        get_char ();

        nx = read_sinteger ();
        dx = read_sinteger ();
        ny = read_sinteger ();
        dy = read_sinteger ();

      } else if (c == '9' && cc == '4') {

        get_char ();

        // label at location 
        ++shapes;

        if (layer < 0) {
          if (layer < -1) {
            warn ("'94' command ignored since no layer was selected");
          }
        } else {

          std::string text = read_string ();

          int rx = read_sinteger ();
          int ry = read_sinteger ();

          double h = 0.0;
          if (! test_semi ()) {
            h = read_double ();
          }

	  unsigned int l = layer;
	  std::string name = read_name ();
	  if (! name.empty ()) {
	    std::pair<bool, unsigned int> ll = m_layer_map.logical (name);
	    if (ll.first) {
	      l = ll.second;
	    }
	  }

          db::Text t (text.c_str (), db::Trans (db::Vector (sf * rx, sf * ry)), db::coord_traits <db::Coord>::rounded (h / m_dbu));
          cell.shapes ((unsigned int) l).insert (t);

        }

      } else if (c == '9' && cc == '5') {

        get_char ();

        // label in box 
        ++shapes;

        if (layer < 0) {
          if (layer < -1) {
            warn ("'95' command ignored since no layer was selected");
          }
        } else {

          std::string text = read_string ();

          //  TODO: box dimensions are ignored currently.
          read_sinteger ();
          read_sinteger ();

          int rx = read_sinteger ();
          int ry = read_sinteger ();

          db::Text t (text.c_str (), db::Trans (db::Vector (sf * rx, sf * ry)));
          cell.shapes ((unsigned int) layer).insert (t);

        }

      } else if (c == '9' && cc == '8') {

        get_char ();

        // Path type (0: flush, 1: round, 2: square)
        path_mode = read_integer ();

      } else if (c == '9' && ! isdigit (cc)) {

        m_cellname = read_string ();
        m_cellname = layout.uniquify_cell_name (m_cellname.c_str ());
        layout.rename_cell (cell.cell_index (), m_cellname.c_str ());

      } else {
        //  ignore the command 
      }

      skip_to_end ();

    } else {

      //  ignore the command 
      warn ("Unknown command ignored");
      skip_to_end ();

    }
    
  }

  //  The cell is considered non-empty if it contains more than one instance, at least one shape or
  //  has at least one "L" command.
  return insts > 1 || shapes > 0 || layer_specs > 0;
}

void 
CIFReader::do_read (db::Layout &layout)
{
  tl::SelfTimer timer (tl::verbosity () >= 21, "File read");

  try {
  
    double sf = 0.01 / m_dbu;
    layout.dbu (m_dbu);

    m_cellname = "{CIF top level}";
    m_next_layer_index = m_layer_map.next_index ();
    m_new_layers.clear ();

    db::Cell &cell = layout.cell (layout.add_cell ());

    if (! read_cell (layout, cell, sf, 0)) {
      // The top cell is empty or contains a single instance: discard it.
      layout.delete_cell (cell.cell_index ());
    } else {
      layout.rename_cell (cell.cell_index (), layout.uniquify_cell_name ("CIF_TOP").c_str ());
    }

    m_cellname = std::string ();

    skip_blanks ();

    if (! m_stream.at_end ()) {
      warn ("E command is followed by more text");
    }

    //  assign layer numbers to new layers
    if (! m_new_layers.empty ()) {

      std::set<std::pair<int, int> > used_ld;
      for (db::Layout::layer_iterator l = layout.begin_layers (); l != layout.end_layers (); ++l) {
        used_ld.insert (std::make_pair((*l).second->layer, (*l).second->datatype));
      }

      //  assign fixed layer numbers for all layers whose name is a fixed number unless there is already a layer with that number
      for (std::map<std::string, unsigned int>::iterator i = m_new_layers.begin (); i != m_new_layers.end (); ) {

        std::map<std::string, unsigned int>::iterator ii = i;
        ++ii;

        int l = -1;
        if (extract_plain_layer (i->first.c_str (), l) && used_ld.find (std::make_pair (l, 0)) == used_ld.end ()) {

          used_ld.insert (std::make_pair (l, 0));

          db::LayerProperties lp;
          lp.layer = l;
          lp.datatype = 0;
          layout.set_properties (i->second, lp);
          m_layer_map.map (lp, i->second);

          m_new_layers.erase (i);

        }

        i = ii;

      }
      
      //  assign fixed layer numbers for all layers whose name is a LxDy or Lx notation unless there is already a layer with that layer/datatype
      for (std::map<std::string, unsigned int>::iterator i = m_new_layers.begin (); i != m_new_layers.end (); ) {

        std::map<std::string, unsigned int>::iterator ii = i;
        ++ii;

        int l = -1, d = -1;
        std::string n;

        if (extract_ld (i->first.c_str (), l, d, n) && used_ld.find (std::make_pair (l, d)) == used_ld.end ()) {

          used_ld.insert (std::make_pair (l, d));

          db::LayerProperties lp;
          lp.layer = l;
          lp.datatype = d;
          lp.name = n;
          layout.set_properties (i->second, lp);
          m_layer_map.map (lp, i->second);

          m_new_layers.erase (i);

        }

        i = ii;

      }

      for (std::map<std::string, unsigned int>::const_iterator i = m_new_layers.begin (); i != m_new_layers.end (); ++i) {
        db::LayerProperties lp;
        lp.name = i->first;
        layout.set_properties (i->second, lp);
        m_layer_map.map (lp, i->second);
      }

    }

  } catch (db::CIFReaderException &ex) {
    throw ex;
  } catch (tl::Exception &ex) {
    error (ex.msg ());
  }
}

}

