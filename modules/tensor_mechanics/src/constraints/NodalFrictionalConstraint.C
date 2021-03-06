//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

// MOOSE includes
#include "NodalFrictionalConstraint.h"
#include "NodalConstraintUtils.h"
#include "MooseMesh.h"
#include "Assembly.h"
#include "SystemBase.h"

#include "libmesh/mesh_inserter_iterator.h"
#include "libmesh/parallel.h"
#include "libmesh/parallel_elem.h"
#include "libmesh/parallel_node.h"

// C++ includes
#include <limits.h>

registerMooseObject("MooseApp", NodalFrictionalConstraint);

template <>
InputParameters
validParams<NodalFrictionalConstraint>()
{
  InputParameters params = validParams<NodalConstraint>();
  params.addClassDescription("Frictional nodal constraint for contact");
  params.addRequiredParam<BoundaryName>("boundary", "The master boundary");
  params.addRequiredParam<BoundaryName>("slave", "The slave boundary");
  params.addRequiredParam<Real>("friction_coefficient",
                                "Friction coefficient for slippage in the normal direction");
  params.addRequiredParam<Real>("normal_force",
                                "Normal force used together with friction_coefficient to compute "
                                "the normal frictional capacity.");
  params.addRequiredParam<Real>("tangential_penalty",
                                "Stiffness of the spring in the tangential direction.");
  return params;
}

NodalFrictionalConstraint::NodalFrictionalConstraint(const InputParameters & parameters)
  : NodalConstraint(parameters),
    _master_boundary_id(getParam<BoundaryName>("boundary")),
    _slave_boundary_id(getParam<BoundaryName>("slave")),
    _normal_force(getParam<Real>("normal_force")),
    _tangential_penalty(getParam<Real>("tangential_penalty")),
    _friction_coefficient(getParam<Real>("friction_coefficient")),
    _u_slave_old(_var.dofValuesOldNeighbor()),
    _u_master_old(_var.dofValuesOld())
{
  updateConstrainedNodes();

  MooseEnum temp_formulation = getParam<MooseEnum>("formulation");
  if (temp_formulation == "penalty")
    _formulation = Moose::Penalty;
  else if (temp_formulation == "kinematic")
    mooseError("NodalFrictionalConstraint: Kinematic formulation is currently not supported for "
               "this constraint.");
  else
    mooseError("Formulation must be set to Penalty.");
}

void
NodalFrictionalConstraint::meshChanged()
{
  updateConstrainedNodes();
}

void
NodalFrictionalConstraint::updateConstrainedNodes()
{
  _master_node_vector.clear();
  _connected_nodes.clear();
  _master_conn.clear();

  std::vector<dof_id_type> slave_nodelist =
      _mesh.getNodeList(_mesh.getBoundaryID(_slave_boundary_id));
  std::vector<dof_id_type> master_nodelist =
      _mesh.getNodeList(_mesh.getBoundaryID(_master_boundary_id));

  // Fill in _connected_nodes, which defines slave nodes in the base class
  for (auto in : slave_nodelist)
  {
    if (_mesh.nodeRef(in).processor_id() == _subproblem.processor_id())
      _connected_nodes.push_back(in);
  }

  // Fill in _master_node_vector, which defines slave nodes in the base class
  for (auto in : master_nodelist)
    _master_node_vector.push_back(in);

  const auto & node_to_elem_map = _mesh.nodeToElemMap();
  std::vector<std::vector<dof_id_type>> elems(_master_node_vector.size());

  // Add elements connected to master node to Ghosted Elements.

  // On a distributed mesh, these elements might have already been
  // remoted, in which case we need to gather them back first.
  if (!_mesh.getMesh().is_serial())
  {
    std::set<Elem *, CompareElemsByLevel> master_elems_to_ghost;
    std::set<Node *> nodes_to_ghost;

    for (unsigned int i = 0; i < master_nodelist.size(); ++i)
    {
      auto node_to_elem_pair = node_to_elem_map.find(_master_node_vector[i]);

      bool found_elems = (node_to_elem_pair != node_to_elem_map.end());

#ifndef NDEBUG
      bool someone_found_elems = found_elems;
      _mesh.getMesh().comm().max(someone_found_elems);
      mooseAssert(someone_found_elems, "Missing entry in node to elem map");
#endif

      if (found_elems)
      {
        for (auto id : node_to_elem_pair->second)
        {
          Elem * elem = _mesh.queryElemPtr(id);
          if (elem)
          {
            master_elems_to_ghost.insert(elem);

            const unsigned int n_nodes = elem->n_nodes();
            for (unsigned int n = 0; n != n_nodes; ++n)
              nodes_to_ghost.insert(elem->node_ptr(n));
          }
        }
      }
    }

    // Send nodes first since elements need them
    _mesh.getMesh().comm().allgather_packed_range(&_mesh.getMesh(),
                                                  nodes_to_ghost.begin(),
                                                  nodes_to_ghost.end(),
                                                  mesh_inserter_iterator<Node>(_mesh.getMesh()));

    _mesh.getMesh().comm().allgather_packed_range(&_mesh.getMesh(),
                                                  master_elems_to_ghost.begin(),
                                                  master_elems_to_ghost.end(),
                                                  mesh_inserter_iterator<Elem>(_mesh.getMesh()));

    _mesh.update(); // Rebuild node_to_elem_map

    // Find elems again now that we know they're there
    const auto & new_node_to_elem_map = _mesh.nodeToElemMap();
    auto node_to_elem_pair = new_node_to_elem_map.find(_master_node_vector[0]);
    bool found_elems = (node_to_elem_pair != new_node_to_elem_map.end());

    if (!found_elems)
      mooseError("Colundn't find any elements connected to master node.");

    for (unsigned int i = 0; i < _master_node_vector.size(); ++i)
      elems[i] = node_to_elem_pair->second;
  }
  else // serial mesh
  {
    for (unsigned int i = 0; i < _master_node_vector.size(); ++i)
    {
      auto node_to_elem_pair = node_to_elem_map.find(_master_node_vector[i]);
      bool found_elems = (node_to_elem_pair != node_to_elem_map.end());

      if (!found_elems)
        mooseError("Couldn't find any elements connected to master node");

      elems[i] = node_to_elem_pair->second;
    }
  }

  for (unsigned int i = 0; i < _master_node_vector.size(); ++i)
  {
    if (elems[i].size() == 0)
      mooseError("Couldn't find any elements connected to master node");

    for (unsigned int j = 0; j < elems[i].size(); ++j)
      _subproblem.addGhostedElem(elems[i][j]);
  }

  // Cache map between slave node and master node
  _connected_nodes.clear();
  _master_conn.clear();
  for (unsigned int j = 0; j < slave_nodelist.size(); ++j)
  {
    if (_mesh.nodeRef(slave_nodelist[j]).processor_id() == _subproblem.processor_id())
    {
      Node & slave_node = _mesh.nodeRef(slave_nodelist[j]);
      for (unsigned int i = 0; i < _master_node_vector.size(); ++i)
      {
        Node & master_node = _mesh.nodeRef(_master_node_vector[i]);
        Real d = (slave_node - master_node).norm();
        if (MooseUtils::absoluteFuzzyEqual(d, 0.0))
        {
          _master_conn.push_back(i);
          _connected_nodes.push_back(slave_nodelist[j]);
          break;
        }
      }
    }
    /*if (_master_conn[j] == std::numeric_limits<unsigned int>::max())
    {
        printf("in here \n");
        mooseError("No master node located at the same elevation as the slave node.");
    }*/
  }
  printf("total slave nodes, master nodes: %lu, %lu \n",
         _master_conn.size(),
         _master_node_vector.size());
}

void
NodalFrictionalConstraint::computeResidual(NumericVector<Number> &
                                           /*residual*/)
{
  std::vector<dof_id_type> masterdof = _var.dofIndices();
  std::vector<dof_id_type> slavedof = _var.dofIndicesNeighbor();
  DenseVector<Number> re(masterdof.size());
  DenseVector<Number> neighbor_re(slavedof.size());

  re.zero();
  neighbor_re.zero();

  for (_i = 0; _i < slavedof.size(); ++_i)
  {
    _j = _master_conn[_i];
    re(_j) += computeQpResidual(Moose::Master);
    neighbor_re(_i) += computeQpResidual(Moose::Slave);
    break;
  }
  _assembly.cacheResidualNodes(re, masterdof);
  _assembly.cacheResidualNodes(neighbor_re, slavedof);
}

Real
NodalFrictionalConstraint::computeQpResidual(Moose::ConstraintType type)
{
  // check whether the tangential spring is already in the yielded state
  Real old_force = (_u_slave_old[_i] - _u_master_old[_j]) * _tangential_penalty;
  if (MooseUtils::absoluteFuzzyGreaterThan(std::abs(old_force),
                                           _friction_coefficient * _normal_force))
    old_force = _friction_coefficient * _normal_force * old_force / std::abs(old_force);

  Real current_force = ((_u_slave[_i] - _u_slave_old[_i]) - (_u_master[_j] - _u_master_old[_j])) *
                           _tangential_penalty +
                       old_force;
  if (MooseUtils::absoluteFuzzyGreaterThan(std::abs(current_force),
                                           _friction_coefficient * _normal_force))
    current_force = _friction_coefficient * _normal_force * current_force / std::abs(current_force);

  switch (type)
  {
    case Moose::Slave:
      return current_force;
    case Moose::Master:
      return -current_force;
  }
  return 0;
}

void
NodalFrictionalConstraint::computeJacobian(SparseMatrix<Number> & /*jacobian*/)
{
  // Calculate Jacobian enteries and cache those entries along with the row and column indices
  std::vector<dof_id_type> slavedof = _var.dofIndicesNeighbor();
  std::vector<dof_id_type> masterdof = _var.dofIndices();

  DenseMatrix<Number> Kee(masterdof.size(), masterdof.size());
  DenseMatrix<Number> Ken(masterdof.size(), slavedof.size());
  DenseMatrix<Number> Kne(slavedof.size(), masterdof.size());
  DenseMatrix<Number> Knn(slavedof.size(), slavedof.size());

  Kee.zero();
  Ken.zero();
  Kne.zero();
  Knn.zero();

  for (_i = 0; _i < slavedof.size(); ++_i)
  {
    _j = _master_conn[_i];
    Kee(_j, _j) += computeQpJacobian(Moose::MasterMaster);
    Ken(_j, _i) += computeQpJacobian(Moose::MasterSlave);
    Kne(_i, _j) += computeQpJacobian(Moose::SlaveMaster);
    Knn(_i, _i) += computeQpJacobian(Moose::SlaveSlave);
  }
  _assembly.cacheJacobianBlock(Kee, masterdof, masterdof, _var.scalingFactor());
  _assembly.cacheJacobianBlock(Ken, masterdof, slavedof, _var.scalingFactor());
  _assembly.cacheJacobianBlock(Kne, slavedof, masterdof, _var.scalingFactor());
  _assembly.cacheJacobianBlock(Knn, slavedof, slavedof, _var.scalingFactor());
}

Real
NodalFrictionalConstraint::computeQpJacobian(Moose::ConstraintJacobianType type)
{
  Real jac = _tangential_penalty;

  // set jacobian to zero if spring has yielded
  Real old_force = (_u_slave_old[_i] - _u_master_old[_j]) * _tangential_penalty;
  if (MooseUtils::absoluteFuzzyGreaterThan(std::abs(old_force),
                                           _friction_coefficient * _normal_force))
    old_force = _friction_coefficient * _normal_force * old_force / std::abs(old_force);

  Real current_force = ((_u_slave[_i] - _u_slave_old[_i]) - (_u_master[_j] - _u_master_old[_j])) *
                           _tangential_penalty +
                       old_force;
  if (MooseUtils::absoluteFuzzyGreaterThan(std::abs(current_force),
                                           _friction_coefficient * _normal_force))
    jac = 0.0;

  switch (type)
  {
    case Moose::SlaveSlave:
      return jac;
    case Moose::SlaveMaster:
      return -jac;
    case Moose::MasterMaster:
      return jac;
    case Moose::MasterSlave:
      return -jac;
    default:
      mooseError("Invalid type");
  }
  return 0.;
}
