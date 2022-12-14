//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "BilinearInterpolation.h"
#include "libmesh/int_range.h"

int BilinearInterpolation::_file_number = 0;

BilinearInterpolation::BilinearInterpolation(const std::vector<Real> & x,
                                             const std::vector<Real> & y,
                                             const ColumnMajorMatrix & z)
  : _xAxis(x), _yAxis(y), _zSurface(z)
{
}

void
BilinearInterpolation::getNeighborIndices(const std::vector<Real> & inArr,
                                          Real x,
                                          int & lowerX,
                                          int & upperX) const
{
  int N = inArr.size();
  if (x <= inArr[0])
  {
    lowerX = 0;
    upperX = 0;
  }
  else if (x >= inArr[N - 1])
  {
    lowerX = N - 1;
    upperX = N - 1;
  }
  else
  {
    for (const auto i : make_range(1, N))
    {
      if (x < inArr[i])
      {
        lowerX = i - 1;
        upperX = i;
        break;
      }
      else if (x == inArr[i])
      {
        lowerX = i;
        upperX = i;
        break;
      }
    }
  }
}

template <typename T>
T
BilinearInterpolation::sample(const T & xcoord, const T & ycoord) const
{
  // first find 4 neighboring points
  int lx = 0; // index of x coordinate of adjacent grid point to left of P
  int ux = 0; // index of x coordinate of adjacent grid point to right of P
  getNeighborIndices(_xAxis, MetaPhysicL::raw_value(xcoord), lx, ux);

  int ly = 0; // index of y coordinate of adjacent grid point below P
  int uy = 0; // index of y coordinate of adjacent grid point above P
  getNeighborIndices(_yAxis, MetaPhysicL::raw_value(ycoord), ly, uy);

  const Real & fQ11 = _zSurface(ly, lx);
  const Real & fQ21 = _zSurface(ly, ux);
  const Real & fQ12 = _zSurface(uy, lx);
  const Real & fQ22 = _zSurface(uy, ux);

  // if point exactly found on a node do not interpolate
  if ((lx == ux) && (ly == uy))
    return fQ11;

  const auto & x = xcoord;
  const auto & y = ycoord;
  const Real & x1 = _xAxis[lx];
  const Real & x2 = _xAxis[ux];
  const Real & y1 = _yAxis[ly];
  const Real & y2 = _yAxis[uy];

  // if xcoord lies exactly on an xAxis node do linear interpolation
  if (lx == ux)
    return fQ11 + (fQ12 - fQ11) * (y - y1) / (y2 - y1);

  // if ycoord lies exactly on an yAxis node do linear interpolation

  if (ly == uy)
    return fQ11 + (fQ21 - fQ11) * (x - x1) / (x2 - x1);

  auto fxy = fQ11 * (x2 - x) * (y2 - y);
  fxy += fQ21 * (x - x1) * (y2 - y);
  fxy += fQ12 * (x2 - x) * (y - y1);
  fxy += fQ22 * (x - x1) * (y - y1);
  fxy /= ((x2 - x1) * (y2 - y1));

  return fxy;
}

template Real BilinearInterpolation::sample<Real>(const Real &, const Real &) const;
template ADReal BilinearInterpolation::sample<ADReal>(const ADReal &, const ADReal &) const;
