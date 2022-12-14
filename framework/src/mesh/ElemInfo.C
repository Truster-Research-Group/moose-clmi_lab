//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "MooseError.h"
#include "FaceInfo.h"
#include "ElemInfo.h"

ElemInfo::ElemInfo(const Elem * const elem)
  : _elem(elem), _volume(_elem->volume()), _centroid(_elem->vertex_average())
{
}

ElemInfo::ElemInfo() : _elem(nullptr) {}

void
ElemInfo::initialize(const ElemInfo & ei, const FaceInfo & fi)
{
  if (!isGhost())
    mooseError("Initialize should only be called on ghost elements!");
  _volume = ei.volume();
  _centroid = 2 * fi.faceCentroid() - ei.centroid();
}
