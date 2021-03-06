#pragma once

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace cache {

/// Utility class to pretty print tables. Rows can be added and at the end
/// printed with fixed with spacing.
class TablePrinter {
public:
  TablePrinter();

  /// Add a column to the table. All calls to AddColumn() must come before any calls to
  /// AddRow().
  void AddColumn(std::string_view label, bool left_align);

  /// Sets the max per column output width (otherwise, columns will be as wide as the
  /// largest value). Values longer than this will be cut off.
  void set_max_output_width(int width);

  /// Add a row to the table. This must have the same width as labels.
  void AddRow(const std::vector<std::string>& row);

  // Add empty row to table
  void AddEmptyRow();

  /// Print to a table with prefix coming before the output.
  std::string ToString(const std::string& prefix = "") const;

private:
  std::vector<std::string> labels_;

  /// For each column, true if the value should be left aligned, right aligned otherwise.
  std::vector<bool> left_align_;

  /// -1 to indicate unlimited.
  int max_output_width_;

  std::vector<std::vector<std::string> > rows_;
  std::vector<int> max_col_widths_;

  /// Helper function to print one row to ss.
  void PrintRow(std::stringstream* ss, const std::vector<std::string>& row,
      const std::vector<int>& widths) const;
};

}

