#include "BasicExternalProblem.h"
#include "AuxiliarySystem.h"

registerMooseObject("MooseTestApp", BasicExternalProblem);

InputParameters
BasicExternalProblem::validParams()
{
  return ExternalProblem::validParams();
}

BasicExternalProblem::BasicExternalProblem(const InputParameters & params) : ExternalProblem(params)
{
}

void
BasicExternalProblem::addExternalVariables()
{
  auto var_params = _factory.getValidParams("MooseVariable");
  var_params.set<MooseEnum>("family") = "MONOMIAL";
  var_params.set<MooseEnum>("order") = "CONSTANT";

  addAuxVariable("MooseVariable", "heat_source", var_params);
  _heat_source_var = _aux->getFieldVariable<Real>(0, "heat_source").number();
}

void
BasicExternalProblem::externalSolve()
{
}

void
BasicExternalProblem::syncSolutions(ExternalProblem::Direction direction)
{
  switch (direction)
  {
    case ExternalProblem::Direction::TO_EXTERNAL_APP:
      break;
    case ExternalProblem::Direction::FROM_EXTERNAL_APP:
    {
      auto & solution = _aux->solution();
      const auto sys_number = _aux->number();
      const auto & mesh = _mesh.getMesh();

      for (unsigned int e = 0; e < mesh.n_elem(); ++e)
      {
        auto elem_ptr = mesh.query_elem_ptr(e);
        if (elem_ptr)
        {
          auto dof_idx = elem_ptr->dof_number(sys_number, _heat_source_var, 0);
          solution.set(dof_idx, 12345);
        }
      }

      solution.close();
      break;
    }
    default:
      mooseError("bad!");
  }
}
