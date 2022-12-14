//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "FancyExtruderGenerator.h"

#include "libmesh/boundary_info.h"
#include "libmesh/function_base.h"
#include "libmesh/cell_prism6.h"
#include "libmesh/cell_prism18.h"
#include "libmesh/cell_hex8.h"
#include "libmesh/cell_hex27.h"
#include "libmesh/cell_tet4.h"
#include "libmesh/cell_tet10.h"
#include "libmesh/face_tri3.h"
#include "libmesh/face_tri6.h"
#include "libmesh/face_quad4.h"
#include "libmesh/face_quad9.h"
#include "libmesh/libmesh_logging.h"
#include "libmesh/mesh_communication.h"
#include "libmesh/mesh_modification.h"
#include "libmesh/mesh_tools.h"
#include "libmesh/parallel.h"
#include "libmesh/remote_elem.h"
#include "libmesh/string_to_enum.h"
#include "libmesh/unstructured_mesh.h"
#include "libmesh/point.h"

#include <numeric>

registerMooseObject("MooseApp", FancyExtruderGenerator);

InputParameters
FancyExtruderGenerator::validParams()
{
  InputParameters params = MeshGenerator::validParams();

  params.addRequiredParam<MeshGeneratorName>("input", "The mesh to extrude");

  params.addClassDescription("Extrudes a 2D mesh into 3D, can have a variable height for each "
                             "elevation, variable number of layers within each elevation and remap "
                             "subdomain_ids and element extra integers within each elevation.");

  params.addRequiredParam<std::vector<Real>>("heights", "The height of each elevation");

  params.addRequiredParam<std::vector<unsigned int>>(
      "num_layers", "The number of layers for each elevation - must be num_elevations in length!");

  params.addParam<std::vector<std::vector<subdomain_id_type>>>(
      "subdomain_swaps",
      "For each row, every two entries are interpreted as a pair of "
      "'from' and 'to' to remap the subdomains for that elevation");

  params.addParam<std::vector<std::string>>(
      "elem_integer_names_to_swap",
      "Array of element extra integer names that need to be swapped during extrusion.");

  params.addParam<std::vector<std::vector<unsigned int>>>(
      "elem_integers_swaps",
      "For each row, every two entries are interpreted as a pair of 'from' and 'to' to remap the "
      "element extra integer for that elevation. If multiple element extra integers need to be "
      "swapped, the enties are stacked based on the order provided in "
      "'elem_integer_names_to_swap'.");

  params.addRequiredParam<Point>(
      "direction",
      "A vector that points in the direction to extrude (note, this will be "
      "normalized internally - so don't worry about it here)");

  params.addParam<boundary_id_type>(
      "top_boundary",
      "The boundary ID to set on the top boundary.  If ommitted one will be generated.");

  params.addParam<boundary_id_type>(
      "bottom_boundary",
      "The boundary ID to set on the bottom boundary.  If omitted one will be generated.");

  return params;
}

FancyExtruderGenerator::FancyExtruderGenerator(const InputParameters & parameters)
  : MeshGenerator(parameters),
    _input(getMesh("input")),
    _heights(getParam<std::vector<Real>>("heights")),
    _num_layers(getParam<std::vector<unsigned int>>("num_layers")),
    _subdomain_swaps(getParam<std::vector<std::vector<subdomain_id_type>>>("subdomain_swaps")),
    _elem_integer_names_to_swap(getParam<std::vector<std::string>>("elem_integer_names_to_swap")),
    _elem_integers_swaps(getParam<std::vector<std::vector<unsigned int>>>("elem_integers_swaps")),
    _direction(getParam<Point>("direction")),
    _has_top_boundary(isParamValid("top_boundary")),
    _top_boundary(isParamValid("top_boundary") ? getParam<boundary_id_type>("top_boundary") : 0),
    _has_bottom_boundary(isParamValid("bottom_boundary")),
    _bottom_boundary(isParamValid("bottom_boundary") ? getParam<boundary_id_type>("bottom_boundary")
                                                     : 0)
{
  if (!_direction.norm())
    paramError("direction", "Must have some length!");

  // Normalize it
  _direction /= _direction.norm();

  const auto num_elevations = _heights.size();

  if (_num_layers.size() != num_elevations)
    paramError("heights", "The length of 'heights' and 'num_layers' must be the same in ", name());

  if (_subdomain_swaps.size() && (_subdomain_swaps.size() != num_elevations))
    paramError("subdomain_swaps",
               "If specified, 'subdomain_swaps' must be the same length as 'heights' in ",
               name());

  _subdomain_swap_pairs.resize(_subdomain_swaps.size());

  // Reprocess the subdomain swaps to make pairs out of them so they are easier to use
  for (unsigned int i = 0; i < _subdomain_swaps.size(); i++)
  {
    const auto & elevation_swaps = _subdomain_swaps[i];
    auto & elevation_swap_pairs = _subdomain_swap_pairs[i];

    if (elevation_swaps.size() % 2)
      paramError("subdomain_swaps",
                 "Row ",
                 i + 1,
                 " of subdomain_swaps in ",
                 name(),
                 " does not contain an even number of entries! Num entries: ",
                 elevation_swaps.size());

    for (unsigned int j = 0; j < elevation_swaps.size(); j += 2)
      elevation_swap_pairs[elevation_swaps[j]] = elevation_swaps[j + 1];
  }

  if (_elem_integers_swaps.size() &&
      (_elem_integers_swaps.size() != num_elevations * _elem_integer_names_to_swap.size()))
    paramError("elem_integers_swaps",
               "If specified, 'elem_integers_swaps' must be the same length as the product of the "
               "length of 'heights' and the length of 'elem_integer_names_to_swap'.");

  _elem_integers_swap_pairs.resize(_elem_integers_swaps.size());
  // Reprocess the elem_integers_swaps to make pairs out of them so they are easier to use
  for (unsigned int i = 0; i < _elem_integer_names_to_swap.size(); i++)
  {
    for (unsigned int j = 0; j < num_elevations; j++)
    {
      const auto & elevation_extra_swaps = _elem_integers_swaps[i * num_elevations + j];
      auto & elevation_extra_swap_pairs = _elem_integers_swap_pairs[i * num_elevations + j];

      if (elevation_extra_swaps.size() % 2)
        paramError("elem_integers_swaps",
                   "Row ",
                   i * num_elevations + j + 1,
                   " of elem_integers_swaps in ",
                   name(),
                   " does not contain an even number of entries! Num entries: ",
                   elevation_extra_swaps.size());
      for (unsigned int k = 0; k < elevation_extra_swaps.size(); k += 2)
        elevation_extra_swap_pairs[elevation_extra_swaps[k]] = elevation_extra_swaps[k + 1];
    }
  }

  bool has_negative_entry = false;
  bool has_positive_entry = false;
  for (const auto & h : _heights)
  {
    if (h > 0.0)
      has_positive_entry = true;
    else
      has_negative_entry = true;
  }

  if (has_negative_entry && has_positive_entry)
    paramError("heights", "Cannot have both positive and negative heights!");
}

std::unique_ptr<MeshBase>
FancyExtruderGenerator::generate()
{
  // Note: bulk of this code originally from libmesh mesh_modification.C
  // Original copyright: Copyright (C) 2002-2019 Benjamin S. Kirk, John W. Peterson, Roy H. Stogner
  // Original license is LGPL so it can be used here.

  auto mesh = buildMeshBaseObject();
  mesh->set_mesh_dimension(_input->mesh_dimension() + 1);

  // Check if the element integer names are existent in the input mesh.
  for (unsigned int i = 0; i < _elem_integer_names_to_swap.size(); i++)
    if (_input->has_elem_integer(_elem_integer_names_to_swap[i]))
      _elem_integer_indices_to_swap.push_back(
          _input->get_elem_integer_index(_elem_integer_names_to_swap[i]));
    else
      paramError("elem_integer_names_to_swap",
                 "Element ",
                 i + 1,
                 " of 'elem_integer_names_to_swap' in is not a valid extra element integer of the "
                 "input mesh.");

  // prepare for transferring extra element integers from orignal mesh to the extruded mesh.
  const unsigned int num_extra_elem_integers = _input->n_elem_integers();
  std::vector<std::string> id_names;

  for (unsigned int i = 0; i < num_extra_elem_integers; i++)
  {
    id_names.push_back(_input->get_elem_integer_name(i));
    if (!mesh->has_elem_integer(id_names[i]))
      mesh->add_elem_integer(id_names[i]);
  }

  // retreive subdomain/sideset/nodeset name maps
  const auto & input_subdomain_map = _input->get_subdomain_name_map();
  const auto & input_sideset_map = _input->get_boundary_info().get_sideset_name_map();
  const auto & input_nodeset_map = _input->get_boundary_info().get_nodeset_name_map();

  std::unique_ptr<MeshBase> input = std::move(_input);

  // If we're using a distributed mesh... then make sure we don't have any remote elements hanging
  // around
  if (!input->is_serial())
    mesh->delete_remote_elements();

  unsigned int total_num_layers = std::accumulate(_num_layers.begin(), _num_layers.end(), 0);

  auto total_num_elevations = _heights.size();

  dof_id_type orig_elem = input->n_elem();
  dof_id_type orig_nodes = input->n_nodes();

#ifdef LIBMESH_ENABLE_UNIQUE_ID
  unique_id_type orig_unique_ids = input->parallel_max_unique_id();
#endif

  unsigned int order = 1;

  BoundaryInfo & boundary_info = mesh->get_boundary_info();
  const BoundaryInfo & input_boundary_info = input->get_boundary_info();

  // We know a priori how many elements we'll need
  mesh->reserve_elem(total_num_layers * orig_elem);

  // For straightforward meshes we need one or two additional layers per
  // element.
  if (input->elements_begin() != input->elements_end() &&
      (*input->elements_begin())->default_order() == SECOND)
    order = 2;
  mesh->comm().max(order);

  mesh->reserve_nodes((order * total_num_layers + 1) * orig_nodes);

  // Container to catch the boundary IDs handed back by the BoundaryInfo object
  std::vector<boundary_id_type> ids_to_copy;

  Point old_distance;
  Point current_distance;

  for (const auto & node : input->node_ptr_range())
  {
    unsigned int current_node_layer = 0;

    old_distance.zero();

    for (unsigned int e = 0; e < total_num_elevations; e++)
    {
      auto num_layers = _num_layers[e];

      auto height = _heights[e];

      for (unsigned int k = 0; k < order * num_layers + (e == 0 ? 1 : 0); ++k)
      {
        // For the first layer we don't need to move
        if (e == 0 && k == 0)
          current_distance.zero();
        else
          current_distance = old_distance + _direction * (height / (Real)num_layers / (Real)order);

        Node * new_node = mesh->add_point(*node + current_distance,
                                          node->id() + (current_node_layer * orig_nodes),
                                          node->processor_id());

#ifdef LIBMESH_ENABLE_UNIQUE_ID
        // Let's give the base of the extruded mesh the same
        // unique_ids as the source mesh, in case anyone finds that
        // a useful map to preserve.
        const unique_id_type uid = (current_node_layer == 0)
                                       ? node->unique_id()
                                       : orig_unique_ids +
                                             (current_node_layer - 1) * (orig_nodes + orig_elem) +
                                             node->id();

        new_node->set_unique_id(uid);
#endif

        input_boundary_info.boundary_ids(node, ids_to_copy);
        boundary_info.add_node(new_node, ids_to_copy);

        old_distance = current_distance;
        current_node_layer++;
      }
    }
  }

  const std::set<boundary_id_type> & side_ids = input_boundary_info.get_side_boundary_ids();

  boundary_id_type next_side_id =
      side_ids.empty() ? 0 : cast_int<boundary_id_type>(*side_ids.rbegin() + 1);

  // side_ids may not include ids from remote elements, in which case
  // some processors may have underestimated the next_side_id; let's
  // fix that.
  input->comm().max(next_side_id);

  for (const auto & elem : input->element_ptr_range())
  {
    const ElemType etype = elem->type();

    // build_extrusion currently only works on coarse meshes
    libmesh_assert(!elem->parent());

    unsigned int current_layer = 0;

    for (unsigned int e = 0; e != total_num_elevations; e++)
    {
      auto num_layers = _num_layers[e];

      for (unsigned int k = 0; k != num_layers; ++k)
      {
        Elem * new_elem;
        bool isFlipped(false);
        switch (etype)
        {
          case EDGE2:
          {
            new_elem = new Quad4;
            new_elem->set_node(0) =
                mesh->node_ptr(elem->node_ptr(0)->id() + (current_layer * orig_nodes));
            new_elem->set_node(1) =
                mesh->node_ptr(elem->node_ptr(1)->id() + (current_layer * orig_nodes));
            new_elem->set_node(2) =
                mesh->node_ptr(elem->node_ptr(1)->id() + ((current_layer + 1) * orig_nodes));
            new_elem->set_node(3) =
                mesh->node_ptr(elem->node_ptr(0)->id() + ((current_layer + 1) * orig_nodes));

            if (elem->neighbor_ptr(0) == remote_elem)
              new_elem->set_neighbor(3, const_cast<RemoteElem *>(remote_elem));
            if (elem->neighbor_ptr(1) == remote_elem)
              new_elem->set_neighbor(1, const_cast<RemoteElem *>(remote_elem));

            break;
          }
          case EDGE3:
          {
            new_elem = new Quad9;
            new_elem->set_node(0) =
                mesh->node_ptr(elem->node_ptr(0)->id() + (2 * current_layer * orig_nodes));
            new_elem->set_node(1) =
                mesh->node_ptr(elem->node_ptr(1)->id() + (2 * current_layer * orig_nodes));
            new_elem->set_node(2) =
                mesh->node_ptr(elem->node_ptr(1)->id() + ((2 * current_layer + 2) * orig_nodes));
            new_elem->set_node(3) =
                mesh->node_ptr(elem->node_ptr(0)->id() + ((2 * current_layer + 2) * orig_nodes));
            new_elem->set_node(4) =
                mesh->node_ptr(elem->node_ptr(2)->id() + (2 * current_layer * orig_nodes));
            new_elem->set_node(5) =
                mesh->node_ptr(elem->node_ptr(1)->id() + ((2 * current_layer + 1) * orig_nodes));
            new_elem->set_node(6) =
                mesh->node_ptr(elem->node_ptr(2)->id() + ((2 * current_layer + 2) * orig_nodes));
            new_elem->set_node(7) =
                mesh->node_ptr(elem->node_ptr(0)->id() + ((2 * current_layer + 1) * orig_nodes));
            new_elem->set_node(8) =
                mesh->node_ptr(elem->node_ptr(2)->id() + ((2 * current_layer + 1) * orig_nodes));

            if (elem->neighbor_ptr(0) == remote_elem)
              new_elem->set_neighbor(3, const_cast<RemoteElem *>(remote_elem));
            if (elem->neighbor_ptr(1) == remote_elem)
              new_elem->set_neighbor(1, const_cast<RemoteElem *>(remote_elem));

            break;
          }
          case TRI3:
          {
            new_elem = new Prism6;
            new_elem->set_node(0) =
                mesh->node_ptr(elem->node_ptr(0)->id() + (current_layer * orig_nodes));
            new_elem->set_node(1) =
                mesh->node_ptr(elem->node_ptr(1)->id() + (current_layer * orig_nodes));
            new_elem->set_node(2) =
                mesh->node_ptr(elem->node_ptr(2)->id() + (current_layer * orig_nodes));
            new_elem->set_node(3) =
                mesh->node_ptr(elem->node_ptr(0)->id() + ((current_layer + 1) * orig_nodes));
            new_elem->set_node(4) =
                mesh->node_ptr(elem->node_ptr(1)->id() + ((current_layer + 1) * orig_nodes));
            new_elem->set_node(5) =
                mesh->node_ptr(elem->node_ptr(2)->id() + ((current_layer + 1) * orig_nodes));

            if (elem->neighbor_ptr(0) == remote_elem)
              new_elem->set_neighbor(1, const_cast<RemoteElem *>(remote_elem));
            if (elem->neighbor_ptr(1) == remote_elem)
              new_elem->set_neighbor(2, const_cast<RemoteElem *>(remote_elem));
            if (elem->neighbor_ptr(2) == remote_elem)
              new_elem->set_neighbor(3, const_cast<RemoteElem *>(remote_elem));

            if (new_elem->volume() < 0.0)
            {
              swapNodesInElem(new_elem, 0, 3);
              swapNodesInElem(new_elem, 1, 4);
              swapNodesInElem(new_elem, 2, 5);
              isFlipped = true;
            }

            break;
          }
          case TRI6:
          {
            new_elem = new Prism18;
            new_elem->set_node(0) =
                mesh->node_ptr(elem->node_ptr(0)->id() + (2 * current_layer * orig_nodes));
            new_elem->set_node(1) =
                mesh->node_ptr(elem->node_ptr(1)->id() + (2 * current_layer * orig_nodes));
            new_elem->set_node(2) =
                mesh->node_ptr(elem->node_ptr(2)->id() + (2 * current_layer * orig_nodes));
            new_elem->set_node(3) =
                mesh->node_ptr(elem->node_ptr(0)->id() + ((2 * current_layer + 2) * orig_nodes));
            new_elem->set_node(4) =
                mesh->node_ptr(elem->node_ptr(1)->id() + ((2 * current_layer + 2) * orig_nodes));
            new_elem->set_node(5) =
                mesh->node_ptr(elem->node_ptr(2)->id() + ((2 * current_layer + 2) * orig_nodes));
            new_elem->set_node(6) =
                mesh->node_ptr(elem->node_ptr(3)->id() + (2 * current_layer * orig_nodes));
            new_elem->set_node(7) =
                mesh->node_ptr(elem->node_ptr(4)->id() + (2 * current_layer * orig_nodes));
            new_elem->set_node(8) =
                mesh->node_ptr(elem->node_ptr(5)->id() + (2 * current_layer * orig_nodes));
            new_elem->set_node(9) =
                mesh->node_ptr(elem->node_ptr(0)->id() + ((2 * current_layer + 1) * orig_nodes));
            new_elem->set_node(10) =
                mesh->node_ptr(elem->node_ptr(1)->id() + ((2 * current_layer + 1) * orig_nodes));
            new_elem->set_node(11) =
                mesh->node_ptr(elem->node_ptr(2)->id() + ((2 * current_layer + 1) * orig_nodes));
            new_elem->set_node(12) =
                mesh->node_ptr(elem->node_ptr(3)->id() + ((2 * current_layer + 2) * orig_nodes));
            new_elem->set_node(13) =
                mesh->node_ptr(elem->node_ptr(4)->id() + ((2 * current_layer + 2) * orig_nodes));
            new_elem->set_node(14) =
                mesh->node_ptr(elem->node_ptr(5)->id() + ((2 * current_layer + 2) * orig_nodes));
            new_elem->set_node(15) =
                mesh->node_ptr(elem->node_ptr(3)->id() + ((2 * current_layer + 1) * orig_nodes));
            new_elem->set_node(16) =
                mesh->node_ptr(elem->node_ptr(4)->id() + ((2 * current_layer + 1) * orig_nodes));
            new_elem->set_node(17) =
                mesh->node_ptr(elem->node_ptr(5)->id() + ((2 * current_layer + 1) * orig_nodes));

            if (elem->neighbor_ptr(0) == remote_elem)
              new_elem->set_neighbor(1, const_cast<RemoteElem *>(remote_elem));
            if (elem->neighbor_ptr(1) == remote_elem)
              new_elem->set_neighbor(2, const_cast<RemoteElem *>(remote_elem));
            if (elem->neighbor_ptr(2) == remote_elem)
              new_elem->set_neighbor(3, const_cast<RemoteElem *>(remote_elem));

            if (new_elem->volume() < 0.0)
            {
              swapNodesInElem(new_elem, 0, 3);
              swapNodesInElem(new_elem, 1, 4);
              swapNodesInElem(new_elem, 2, 5);
              swapNodesInElem(new_elem, 6, 12);
              swapNodesInElem(new_elem, 7, 13);
              swapNodesInElem(new_elem, 8, 14);
              isFlipped = true;
            }

            break;
          }
          case QUAD4:
          {
            new_elem = new Hex8;
            new_elem->set_node(0) =
                mesh->node_ptr(elem->node_ptr(0)->id() + (current_layer * orig_nodes));
            new_elem->set_node(1) =
                mesh->node_ptr(elem->node_ptr(1)->id() + (current_layer * orig_nodes));
            new_elem->set_node(2) =
                mesh->node_ptr(elem->node_ptr(2)->id() + (current_layer * orig_nodes));
            new_elem->set_node(3) =
                mesh->node_ptr(elem->node_ptr(3)->id() + (current_layer * orig_nodes));
            new_elem->set_node(4) =
                mesh->node_ptr(elem->node_ptr(0)->id() + ((current_layer + 1) * orig_nodes));
            new_elem->set_node(5) =
                mesh->node_ptr(elem->node_ptr(1)->id() + ((current_layer + 1) * orig_nodes));
            new_elem->set_node(6) =
                mesh->node_ptr(elem->node_ptr(2)->id() + ((current_layer + 1) * orig_nodes));
            new_elem->set_node(7) =
                mesh->node_ptr(elem->node_ptr(3)->id() + ((current_layer + 1) * orig_nodes));

            if (elem->neighbor_ptr(0) == remote_elem)
              new_elem->set_neighbor(1, const_cast<RemoteElem *>(remote_elem));
            if (elem->neighbor_ptr(1) == remote_elem)
              new_elem->set_neighbor(2, const_cast<RemoteElem *>(remote_elem));
            if (elem->neighbor_ptr(2) == remote_elem)
              new_elem->set_neighbor(3, const_cast<RemoteElem *>(remote_elem));
            if (elem->neighbor_ptr(3) == remote_elem)
              new_elem->set_neighbor(4, const_cast<RemoteElem *>(remote_elem));

            if (new_elem->volume() < 0.0)
            {
              swapNodesInElem(new_elem, 0, 4);
              swapNodesInElem(new_elem, 1, 5);
              swapNodesInElem(new_elem, 2, 6);
              swapNodesInElem(new_elem, 3, 7);
              isFlipped = true;
            }

            break;
          }
          case QUAD9:
          {
            new_elem = new Hex27;
            new_elem->set_node(0) =
                mesh->node_ptr(elem->node_ptr(0)->id() + (2 * current_layer * orig_nodes));
            new_elem->set_node(1) =
                mesh->node_ptr(elem->node_ptr(1)->id() + (2 * current_layer * orig_nodes));
            new_elem->set_node(2) =
                mesh->node_ptr(elem->node_ptr(2)->id() + (2 * current_layer * orig_nodes));
            new_elem->set_node(3) =
                mesh->node_ptr(elem->node_ptr(3)->id() + (2 * current_layer * orig_nodes));
            new_elem->set_node(4) =
                mesh->node_ptr(elem->node_ptr(0)->id() + ((2 * current_layer + 2) * orig_nodes));
            new_elem->set_node(5) =
                mesh->node_ptr(elem->node_ptr(1)->id() + ((2 * current_layer + 2) * orig_nodes));
            new_elem->set_node(6) =
                mesh->node_ptr(elem->node_ptr(2)->id() + ((2 * current_layer + 2) * orig_nodes));
            new_elem->set_node(7) =
                mesh->node_ptr(elem->node_ptr(3)->id() + ((2 * current_layer + 2) * orig_nodes));
            new_elem->set_node(8) =
                mesh->node_ptr(elem->node_ptr(4)->id() + (2 * current_layer * orig_nodes));
            new_elem->set_node(9) =
                mesh->node_ptr(elem->node_ptr(5)->id() + (2 * current_layer * orig_nodes));
            new_elem->set_node(10) =
                mesh->node_ptr(elem->node_ptr(6)->id() + (2 * current_layer * orig_nodes));
            new_elem->set_node(11) =
                mesh->node_ptr(elem->node_ptr(7)->id() + (2 * current_layer * orig_nodes));
            new_elem->set_node(12) =
                mesh->node_ptr(elem->node_ptr(0)->id() + ((2 * current_layer + 1) * orig_nodes));
            new_elem->set_node(13) =
                mesh->node_ptr(elem->node_ptr(1)->id() + ((2 * current_layer + 1) * orig_nodes));
            new_elem->set_node(14) =
                mesh->node_ptr(elem->node_ptr(2)->id() + ((2 * current_layer + 1) * orig_nodes));
            new_elem->set_node(15) =
                mesh->node_ptr(elem->node_ptr(3)->id() + ((2 * current_layer + 1) * orig_nodes));
            new_elem->set_node(16) =
                mesh->node_ptr(elem->node_ptr(4)->id() + ((2 * current_layer + 2) * orig_nodes));
            new_elem->set_node(17) =
                mesh->node_ptr(elem->node_ptr(5)->id() + ((2 * current_layer + 2) * orig_nodes));
            new_elem->set_node(18) =
                mesh->node_ptr(elem->node_ptr(6)->id() + ((2 * current_layer + 2) * orig_nodes));
            new_elem->set_node(19) =
                mesh->node_ptr(elem->node_ptr(7)->id() + ((2 * current_layer + 2) * orig_nodes));
            new_elem->set_node(20) =
                mesh->node_ptr(elem->node_ptr(8)->id() + (2 * current_layer * orig_nodes));
            new_elem->set_node(21) =
                mesh->node_ptr(elem->node_ptr(4)->id() + ((2 * current_layer + 1) * orig_nodes));
            new_elem->set_node(22) =
                mesh->node_ptr(elem->node_ptr(5)->id() + ((2 * current_layer + 1) * orig_nodes));
            new_elem->set_node(23) =
                mesh->node_ptr(elem->node_ptr(6)->id() + ((2 * current_layer + 1) * orig_nodes));
            new_elem->set_node(24) =
                mesh->node_ptr(elem->node_ptr(7)->id() + ((2 * current_layer + 1) * orig_nodes));
            new_elem->set_node(25) =
                mesh->node_ptr(elem->node_ptr(8)->id() + ((2 * current_layer + 2) * orig_nodes));
            new_elem->set_node(26) =
                mesh->node_ptr(elem->node_ptr(8)->id() + ((2 * current_layer + 1) * orig_nodes));

            if (elem->neighbor_ptr(0) == remote_elem)
              new_elem->set_neighbor(1, const_cast<RemoteElem *>(remote_elem));
            if (elem->neighbor_ptr(1) == remote_elem)
              new_elem->set_neighbor(2, const_cast<RemoteElem *>(remote_elem));
            if (elem->neighbor_ptr(2) == remote_elem)
              new_elem->set_neighbor(3, const_cast<RemoteElem *>(remote_elem));
            if (elem->neighbor_ptr(3) == remote_elem)
              new_elem->set_neighbor(4, const_cast<RemoteElem *>(remote_elem));

            if (new_elem->volume() < 0.0)
            {
              swapNodesInElem(new_elem, 0, 4);
              swapNodesInElem(new_elem, 1, 5);
              swapNodesInElem(new_elem, 2, 6);
              swapNodesInElem(new_elem, 3, 7);
              swapNodesInElem(new_elem, 8, 16);
              swapNodesInElem(new_elem, 9, 17);
              swapNodesInElem(new_elem, 10, 18);
              swapNodesInElem(new_elem, 11, 19);
              swapNodesInElem(new_elem, 20, 25);
              isFlipped = true;
            }

            break;
          }
          default:
          {
            libmesh_not_implemented();
            break;
          }
        }

        new_elem->set_id(elem->id() + (current_layer * orig_elem));
        new_elem->processor_id() = elem->processor_id();

#ifdef LIBMESH_ENABLE_UNIQUE_ID
        // Let's give the base of the extruded mesh the same
        // unique_ids as the source mesh, in case anyone finds that
        // a useful map to preserve.
        const unique_id_type uid = (current_layer == 0)
                                       ? elem->unique_id()
                                       : orig_unique_ids +
                                             (current_layer - 1) * (orig_nodes + orig_elem) +
                                             orig_nodes + elem->id();

        new_elem->set_unique_id(uid);
#endif

        // maintain the subdomain_id
        new_elem->subdomain_id() = elem->subdomain_id();

        if (_subdomain_swap_pairs.size())
        {
          auto & elevation_swap_pairs = _subdomain_swap_pairs[e];

          auto new_id_it = elevation_swap_pairs.find(elem->subdomain_id());

          if (new_id_it != elevation_swap_pairs.end())
            new_elem->subdomain_id() = new_id_it->second;
        }

        new_elem = mesh->add_elem(new_elem);

        // maintain extra integers
        for (unsigned int i = 0; i < num_extra_elem_integers; i++)
          new_elem->set_extra_integer(i, elem->get_extra_integer(i));

        if (_elem_integers_swap_pairs.size())
        {
          for (unsigned int i = 0; i < _elem_integer_indices_to_swap.size(); i++)
          {
            auto & elevation_extra_swap_pairs = _elem_integers_swap_pairs[i * _heights.size() + e];

            auto new_extra_id_it = elevation_extra_swap_pairs.find(
                elem->get_extra_integer(_elem_integer_indices_to_swap[i]));

            if (new_extra_id_it != elevation_extra_swap_pairs.end())
              new_elem->set_extra_integer(_elem_integer_indices_to_swap[i],
                                          new_extra_id_it->second);
          }
        }

        // Copy any old boundary ids on all sides
        for (auto s : elem->side_index_range())
        {
          input_boundary_info.boundary_ids(elem, s, ids_to_copy);

          if (new_elem->dim() == 3)
          {
            // For 2D->3D extrusion, we give the boundary IDs
            // for side s on the old element to side s+1 on the
            // new element.  This is just a happy coincidence as
            // far as I can tell...
            boundary_info.add_side(new_elem, cast_int<unsigned short>(s + 1), ids_to_copy);
          }
          else
          {
            // For 1D->2D extrusion, the boundary IDs map as:
            // Old elem -> New elem
            // 0        -> 3
            // 1        -> 1
            libmesh_assert_less(s, 2);
            const unsigned short sidemap[2] = {3, 1};
            boundary_info.add_side(new_elem, sidemap[s], ids_to_copy);
          }
        }

        // Give new boundary ids to bottom and top
        if (current_layer == 0)
        {
          const unsigned short top_id =
              new_elem->dim() == 3 ? cast_int<unsigned short>(elem->n_sides() + 1) : 2;
          if (_has_bottom_boundary)
            boundary_info.add_side(new_elem, isFlipped ? top_id : 0, _bottom_boundary);
          else
            boundary_info.add_side(new_elem, isFlipped ? top_id : 0, next_side_id);
        }

        if (current_layer == total_num_layers - 1)
        {
          // For 2D->3D extrusion, the "top" ID is 1+the original
          // element's number of sides.  For 1D->2D extrusion, the
          // "top" ID is side 2.
          const unsigned short top_id =
              new_elem->dim() == 3 ? cast_int<unsigned short>(elem->n_sides() + 1) : 2;

          if (_has_top_boundary)
            boundary_info.add_side(new_elem, isFlipped ? 0 : top_id, _top_boundary);
          else
            boundary_info.add_side(
                new_elem, isFlipped ? 0 : top_id, cast_int<boundary_id_type>(next_side_id + 1));
        }

        current_layer++;
      }
    }
  }

  // Update the value of next_unique_id based on newly created nodes and elements
  // Note: Number of element layers is one less than number of node layers
  unsigned int total_new_node_layers = total_num_layers * order;
  unsigned int new_unique_ids = orig_unique_ids + (total_new_node_layers - 1) * orig_elem +
                                total_new_node_layers * orig_nodes;
  mesh->set_next_unique_id(new_unique_ids);

  // Copy all the subdomain/sideset/nodeset name maps to the extruded mesh
  if (!input_subdomain_map.empty())
    mesh->set_subdomain_name_map().insert(input_subdomain_map.begin(), input_subdomain_map.end());
  if (!input_sideset_map.empty())
    mesh->get_boundary_info().set_sideset_name_map().insert(input_sideset_map.begin(),
                                                            input_sideset_map.end());
  if (!input_nodeset_map.empty())
    mesh->get_boundary_info().set_nodeset_name_map().insert(input_nodeset_map.begin(),
                                                            input_nodeset_map.end());

  mesh->set_isnt_prepared();

  return mesh;
}

void
FancyExtruderGenerator::swapNodesInElem(Elem * elem, const unsigned int nd1, const unsigned int nd2)
{
  Node * n_temp = elem->node_ptr(nd1);
  elem->set_node(nd1) = elem->node_ptr(nd2);
  elem->set_node(nd2) = n_temp;
}
