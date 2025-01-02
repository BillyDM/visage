/* Copyright Vital Audio, LLC
 *
 * visage is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * visage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with visage.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "layout.h"

namespace visage {
  std::vector<Bounds> Layout::flexChildGroup(const std::vector<const Layout*>& children,
                                             Bounds bounds, float dpi_scale) {
    int width = bounds.width();
    int height = bounds.height();
    int dim = flex_rows_ ? 1 : 0;
    int cross_dim = 1 - dim;

    int flex_area = flex_rows_ ? height : width;
    int flex_gap = flex_gap_.computeWithDefault(dpi_scale, width, height);
    flex_area -= flex_gap * (children.size() - 1);
    float total_flex_grow = 0.0f;
    float total_flex_shrink = 0.0f;

    std::vector<int> dimensions, margins_before, margins_after;
    dimensions.reserve(children.size());
    margins_before.reserve(children.size());
    margins_after.reserve(children.size());
    for (const Layout* child : children) {
      int margin_before = child->margin_before_[dim].computeWithDefault(dpi_scale, width, height);
      int margin_after = child->margin_after_[dim].computeWithDefault(dpi_scale, width, height);
      int dimension = child->dimensions_[dim].computeWithDefault(dpi_scale, width, height);
      flex_area -= dimension + margin_before + margin_after;

      dimensions.push_back(dimension);
      margins_before.push_back(margin_before);
      margins_after.push_back(margin_after);
      total_flex_grow += child->flex_grow_;
      total_flex_shrink += child->flex_shrink_ * dimension;
    }

    if (flex_area > 0) {
      for (int i = 0; i < children.size(); ++i) {
        if (children[i]->flex_grow_) {
          int delta = std::round(flex_area * children[i]->flex_grow_ / total_flex_grow);
          dimensions[i] += delta;
          flex_area -= delta;
          total_flex_grow -= children[i]->flex_grow_;
        }
      }
    }

    if (flex_area < 0) {
      for (int i = 0; i < children.size(); ++i) {
        if (children[i]->flex_shrink_) {
          int delta = std::round(flex_area * children[i]->flex_shrink_ * dimensions[i] / total_flex_shrink);
          delta = std::max(delta, -dimensions[i]);
          total_flex_shrink -= children[i]->flex_shrink_ * dimensions[i];
          dimensions[i] += delta;
          flex_area -= delta;
        }
      }
    }

    std::vector<Bounds> results;
    results.reserve(children.size());

    int position = 0;
    int cross_area = flex_rows_ ? width : height;
    for (int i = 0; i < children.size(); ++i) {
      int cross_before = children[i]->margin_before_[cross_dim].computeWithDefault(dpi_scale, width, height);
      int cross_after = children[i]->margin_after_[cross_dim].computeWithDefault(dpi_scale, width, height);
      int default_cross_size = 0;

      ItemAlignment alignment = children[i]->self_alignment_;
      if (alignment == ItemAlignment::NotSet)
        alignment = item_alignment_;

      float cross_alignment_mult = 0.0f;
      if (alignment == ItemAlignment::Stretch)
        default_cross_size = cross_area - cross_before - cross_after;
      else if (alignment == ItemAlignment::Center)
        cross_alignment_mult = 0.5f;
      else if (alignment == ItemAlignment::End)
        cross_alignment_mult = 1.0f;

      int cross_size = children[i]->dimensions_[cross_dim].computeWithDefault(dpi_scale, width, height,
                                                                              default_cross_size);
      int cross_offset = cross_alignment_mult * (cross_area - cross_before - cross_size - cross_after);
      position += margins_before[i];
      results.push_back({ position, cross_before + cross_offset, dimensions[i], cross_size });
      position += dimensions[i] + margins_after[i] + flex_gap;
    }

    if (flex_reverse_direction_) {
      int flex_area = flex_rows_ ? height : width;
      for (Bounds& result : results)
        result.setX(flex_area - result.right());
    }

    if (flex_rows_) {
      for (Bounds& result : results)
        result.flipDimensions();
    }

    for (Bounds& result : results)
      result = result + Point(bounds.x(), bounds.y());

    return results;
  }

  std::vector<int> Layout::alignCrossPositions(std::vector<int>& sizes, int cross_area, int gap) {
    int cross_total = gap * (sizes.size() - 1);
    for (int size : sizes)
      cross_total += size;

    int cross_extra_space = cross_area - cross_total;
    std::vector<int> cross_positions;

    if (wrap_alignment_ == WrapAlignment::Stretch) {
      int position = 0;
      cross_extra_space = std::max(cross_extra_space, 0);

      for (int i = 0; i < sizes.size(); ++i) {
        int remaining = sizes.size() - i;
        int add = cross_extra_space / remaining;
        cross_extra_space -= add;
        sizes[i] += add;
        cross_positions.push_back(position);
        position += sizes[i] + gap;
      }
      return cross_positions;
    }

    int position = 0;
    if (wrap_alignment_ == WrapAlignment::Center)
      position = cross_extra_space / 2;
    else if (wrap_alignment_ == WrapAlignment::End)
      position = cross_extra_space;

    cross_extra_space = std::max(cross_extra_space, 0);

    int border = 0;
    if (wrap_alignment_ == WrapAlignment::SpaceAround)
      border = cross_extra_space / sizes.size();
    else if (wrap_alignment_ == WrapAlignment::SpaceEvenly)
      border = (2 * cross_extra_space) / (sizes.size() + 1);
    else if (wrap_alignment_ != WrapAlignment::SpaceBetween)
      cross_extra_space = 0;

    position += border / 2;
    cross_extra_space -= border;

    for (int i = 0; i < sizes.size(); ++i) {
      int space = 0;
      int remaining = sizes.size() - i - 1;
      if (remaining > 0) {
        space = cross_extra_space / remaining;
        cross_extra_space -= space;
      }
      cross_positions.push_back(position);
      position += sizes[i] + gap + space;
    }

    return cross_positions;
  }

  std::vector<Bounds> Layout::flexChildWrap(const std::vector<const Layout*>& children,
                                            Bounds bounds, float dpi_scale) {
    int width = bounds.width();
    int height = bounds.height();
    int dim = flex_rows_ ? 1 : 0;
    int cross_dim = 1 - dim;

    int total_flex_area = flex_rows_ ? height : width;
    int flex_area = total_flex_area;
    int cross_max = 0;
    int flex_gap = flex_gap_.computeWithDefault(dpi_scale, width, height);

    std::vector<int> breaks;
    std::vector<int> cross_sizes;

    for (int i = 0; i < children.size(); ++i) {
      const Layout* child = children[i];
      int dimension = child->dimensions_[dim].computeWithDefault(dpi_scale, width, height);
      int margin_before = child->margin_before_[dim].computeWithDefault(dpi_scale, width, height);
      int margin_after = child->margin_after_[dim].computeWithDefault(dpi_scale, width, height);
      int total = dimension + margin_before + margin_after;
      flex_area -= total;

      if (flex_area < 0 && i) {
        cross_sizes.push_back(cross_max);
        breaks.push_back(i);
        flex_area = total_flex_area - total;
        cross_max = 0;
      }

      int cross_amount = child->dimensions_[cross_dim].computeWithDefault(dpi_scale, width, height) +
                         child->margin_before_[cross_dim].computeWithDefault(dpi_scale, width, height) +
                         child->margin_after_[cross_dim].computeWithDefault(dpi_scale, width, height);
      cross_max = std::max(cross_amount, cross_max);

      flex_area -= flex_gap;
    }

    int group_index = 0;
    breaks.push_back(children.size());
    cross_sizes.push_back(cross_max);
    int cross_area = flex_rows_ ? width : height;
    std::vector<int> cross_positions = alignCrossPositions(cross_sizes, cross_area, flex_gap);

    std::vector<Bounds> results;
    results.reserve(children.size());
    for (int i = 0; i < breaks.size(); ++i) {
      Bounds group_bounds;
      if (flex_rows_)
        group_bounds = { bounds.x() + cross_positions[i], bounds.y(), cross_sizes[i], bounds.height() };
      else
        group_bounds = { bounds.x(), bounds.y() + cross_positions[i], bounds.width(), cross_sizes[i] };

      auto group = std::vector<const Layout*>(children.begin() + group_index,
                                              children.begin() + breaks[i]);
      group_index = breaks[i];
      std::vector<Bounds> bounds = flexChildGroup(group, group_bounds, dpi_scale);
      results.insert(results.end(), bounds.begin(), bounds.end());
    }

    if (flex_wrap_ < 0) {
      for (Bounds& result : results)
        result.setX(bounds.x() + bounds.right() - result.right());
    }

    return results;
  }
}