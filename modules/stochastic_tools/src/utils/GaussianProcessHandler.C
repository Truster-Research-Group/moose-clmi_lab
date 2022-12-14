//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "GaussianProcessHandler.h"
#include "FEProblemBase.h"

#include <petsctao.h>
#include <petscdmda.h>

#include "libmesh/petsc_vector.h"
#include "libmesh/petsc_matrix.h"

#include <math.h>

namespace StochasticTools
{
GaussianProcessHandler::GaussianProcessHandler() : _tao_comm(MPI_COMM_SELF) {}

void
GaussianProcessHandler::initialize(CovarianceFunctionBase * covariance_function,
                                   const std::vector<std::string> params_to_tune,
                                   std::vector<Real> min,
                                   std::vector<Real> max)
{
  linkCovarianceFunction(covariance_function);
  generateTuningMap(params_to_tune, min, max);
}

void
GaussianProcessHandler::linkCovarianceFunction(CovarianceFunctionBase * covariance_function)
{
  _covariance_function = covariance_function;
  _covar_type = _covariance_function->type();
}

void
GaussianProcessHandler::setupCovarianceMatrix(const RealEigenMatrix & training_params,
                                              const RealEigenMatrix & training_data,
                                              MooseEnum opt_type,
                                              std::string tao_options,
                                              bool show_tao)
{
  _K.resize(training_params.rows(), training_params.rows());

  // This already accounts for future addition of an ADAM optimizes
  if (opt_type == "tao")
    if (tuneHyperParamsTAO(training_params, training_data, tao_options, show_tao))
      ::mooseError("PETSc/TAO error in hyperparameter tuning.");

  _covariance_function->computeCovarianceMatrix(_K, training_params, training_params, true);

  // Compute the Cholesly decomposition and inverse action of the covariance matrix
  setupStoredMatrices(training_data);

  _covariance_function->buildHyperParamMap(_hyperparam_map, _hyperparam_vec_map);
}

void
GaussianProcessHandler::setupStoredMatrices(const RealEigenMatrix & input)
{
  _K_cho_decomp = _K.llt();
  _K_results_solve = _K_cho_decomp.solve(input);
}

void
GaussianProcessHandler::generateTuningMap(const std::vector<std::string> params_to_tune,
                                          std::vector<Real> min_vector,
                                          std::vector<Real> max_vector)
{
  _num_tunable = 0;

  bool upper_bounds_specified = false;
  bool lower_bounds_specified = false;
  if (min_vector.size())
    lower_bounds_specified = true;
  if (max_vector.size())
    upper_bounds_specified = true;

  for (unsigned int param_i = 0; param_i < params_to_tune.size(); ++param_i)
  {
    const auto & hp = params_to_tune[param_i];
    if (_covariance_function->isTunable(hp))
    {
      unsigned int size;
      Real min;
      Real max;
      // Get size and default min/max
      _covariance_function->getTuningData(hp, size, min, max);
      // Check for overridden min/max
      min = lower_bounds_specified ? min_vector[param_i] : min;
      max = upper_bounds_specified ? max_vector[param_i] : max;
      // Save data in tuple
      _tuning_data[hp] = std::make_tuple(_num_tunable, size, min, max);
      _num_tunable += size;
    }
  }
}

void
GaussianProcessHandler::standardizeParameters(RealEigenMatrix & data, bool keep_moments)
{
  if (!keep_moments)
    _param_standardizer.computeSet(data);
  _param_standardizer.getStandardized(data);
}

void
GaussianProcessHandler::standardizeData(RealEigenMatrix & data, bool keep_moments)
{
  if (!keep_moments)
    _data_standardizer.computeSet(data);
  _data_standardizer.getStandardized(data);
}

PetscErrorCode
GaussianProcessHandler::tuneHyperParamsTAO(const RealEigenMatrix & training_params,
                                           const RealEigenMatrix & training_data,
                                           std::string tao_options,
                                           bool show_tao)
{
  PetscErrorCode ierr;
  Tao tao;

  _training_params = &training_params;
  _training_data = &training_data;

  // Setup Tao optimization problem
  ierr = TaoCreate(_tao_comm.get(), &tao);
  CHKERRQ(ierr);
  ierr = PetscOptionsSetValue(NULL, "-tao_type", "bncg");
  CHKERRQ(ierr);
  ierr = PetscOptionsInsertString(NULL, tao_options.c_str());
  CHKERRQ(ierr);
  ierr = TaoSetFromOptions(tao);
  CHKERRQ(ierr);

  // Define petsc vetor to hold tunalbe hyper-params
  libMesh::PetscVector<Number> theta(_tao_comm, _num_tunable);
  ierr = formInitialGuessTAO(theta.vec());
  CHKERRQ(ierr);
  ierr = TaoSetInitialVector(tao, theta.vec());
  CHKERRQ(ierr);

  // Get Hyperparameter bounds.
  libMesh::PetscVector<Number> lower(_tao_comm, _num_tunable);
  libMesh::PetscVector<Number> upper(_tao_comm, _num_tunable);
  buildHyperParamBoundsTAO(lower, upper);
  CHKERRQ(ierr);
  ierr = TaoSetVariableBounds(tao, lower.vec(), upper.vec());
  CHKERRQ(ierr);

  // Set Objective and Graident Callback ierr =
  TaoSetObjectiveAndGradientRoutine(tao, formFunctionGradientWrapper, (void *)this);
  CHKERRQ(ierr);

  // Solve
  ierr = TaoSolve(tao);
  CHKERRQ(ierr);
  //
  if (show_tao)
  {
    ierr = TaoView(tao, PETSC_VIEWER_STDOUT_WORLD);
    theta.print();
  }

  _covariance_function->loadHyperParamMap(_hyperparam_map, _hyperparam_vec_map);

  ierr = TaoDestroy(&tao);
  CHKERRQ(ierr);

  return 0;
}

PetscErrorCode
GaussianProcessHandler::formInitialGuessTAO(Vec theta_vec)
{
  libMesh::PetscVector<Number> theta(theta_vec, _tao_comm);
  _covariance_function->buildHyperParamMap(_hyperparam_map, _hyperparam_vec_map);
  mapToPetscVec(_tuning_data, _hyperparam_map, _hyperparam_vec_map, theta);
  return 0;
}

void
GaussianProcessHandler::buildHyperParamBoundsTAO(libMesh::PetscVector<Number> & theta_l,
                                                 libMesh::PetscVector<Number> & theta_u) const
{
  for (auto iter = _tuning_data.begin(); iter != _tuning_data.end(); ++iter)
  {
    for (unsigned int ii = 0; ii < std::get<1>(iter->second); ++ii)
    {
      theta_l.set(std::get<0>(iter->second) + ii, std::get<2>(iter->second));
      theta_u.set(std::get<0>(iter->second) + ii, std::get<3>(iter->second));
    }
  }
}

PetscErrorCode
GaussianProcessHandler::formFunctionGradientWrapper(
    Tao tao, Vec theta_vec, PetscReal * f, Vec grad_vec, void * ptr)
{
  GaussianProcessHandler * GP_ptr = (GaussianProcessHandler *)ptr;
  GP_ptr->formFunctionGradient(tao, theta_vec, f, grad_vec);
  return 0;
}

void
GaussianProcessHandler::formFunctionGradient(Tao /*tao*/,
                                             Vec theta_vec,
                                             PetscReal * f,
                                             Vec grad_vec)
{
  libMesh::PetscVector<Number> theta(theta_vec, _tao_comm);
  libMesh::PetscVector<Number> grad(grad_vec, _tao_comm);

  petscVecToMap(_tuning_data, _hyperparam_map, _hyperparam_vec_map, theta);
  _covariance_function->loadHyperParamMap(_hyperparam_map, _hyperparam_vec_map);
  _covariance_function->computeCovarianceMatrix(_K, *_training_params, *_training_params, true);
  setupStoredMatrices(*_training_data);

  // testing auto tuning
  RealEigenMatrix dKdhp(_training_params->rows(), _training_params->rows());
  RealEigenMatrix alpha = _K_results_solve * _K_results_solve.transpose();
  for (auto iter = _tuning_data.begin(); iter != _tuning_data.end(); ++iter)
  {
    std::string hyper_param_name = iter->first;
    for (unsigned int ii = 0; ii < std::get<1>(iter->second); ++ii)
    {
      _covariance_function->computedKdhyper(dKdhp, *_training_params, hyper_param_name, ii);
      RealEigenMatrix tmp = alpha * dKdhp - _K_cho_decomp.solve(dKdhp);
      grad.set(std::get<0>(iter->second) + ii, -tmp.trace() / 2.0);
    }
  }
  //
  Real log_likelihood = 0;
  log_likelihood += -(_training_data->transpose() * _K_results_solve)(0, 0);
  log_likelihood += -std::log(_K.determinant());
  log_likelihood += -_training_data->rows() * std::log(2 * M_PI);
  log_likelihood = -log_likelihood / 2;
  *f = log_likelihood;
}

void
GaussianProcessHandler::mapToPetscVec(
    const std::unordered_map<std::string, std::tuple<unsigned int, unsigned int, Real, Real>> &
        tuning_data,
    const std::unordered_map<std::string, Real> & scalar_map,
    const std::unordered_map<std::string, std::vector<Real>> & vector_map,
    libMesh::PetscVector<Number> & petsc_vec)
{
  for (auto iter = tuning_data.begin(); iter != tuning_data.end(); ++iter)
  {
    std::string param_name = iter->first;
    const auto scalar_it = scalar_map.find(param_name);
    if (scalar_it != scalar_map.end())
      petsc_vec.set(std::get<0>(iter->second), scalar_it->second);
    else
    {
      const auto vector_it = vector_map.find(param_name);
      if (vector_it != vector_map.end())
        for (unsigned int ii = 0; ii < std::get<1>(iter->second); ++ii)
          petsc_vec.set(std::get<0>(iter->second) + ii, (vector_it->second)[ii]);
    }
  }
}

void
GaussianProcessHandler::petscVecToMap(
    const std::unordered_map<std::string, std::tuple<unsigned int, unsigned int, Real, Real>> &
        tuning_data,
    std::unordered_map<std::string, Real> & scalar_map,
    std::unordered_map<std::string, std::vector<Real>> & vector_map,
    const libMesh::PetscVector<Number> & petsc_vec)
{
  for (auto iter = tuning_data.begin(); iter != tuning_data.end(); ++iter)
  {
    std::string param_name = iter->first;
    if (scalar_map.find(param_name) != scalar_map.end())
      scalar_map[param_name] = petsc_vec(std::get<0>(iter->second));
    else if (vector_map.find(param_name) != vector_map.end())
      for (unsigned int ii = 0; ii < std::get<1>(iter->second); ++ii)
        vector_map[param_name][ii] = petsc_vec(std::get<0>(iter->second) + ii);
  }
}

} // StochasticTools namespace

template <>
void
dataStore(std::ostream & stream, Eigen::LLT<RealEigenMatrix> & decomp, void * context)
{
  // Store the L matrix as opposed to the full matrix to avoid compounding
  // roundoff error and decomposition error
  RealEigenMatrix L(decomp.matrixL());
  dataStore(stream, L, context);
}

template <>
void
dataLoad(std::istream & stream, Eigen::LLT<RealEigenMatrix> & decomp, void * context)
{
  RealEigenMatrix L;
  dataLoad(stream, L, context);
  decomp.compute(L * L.transpose());
}

template <>
void
dataStore(std::ostream & stream, StochasticTools::GaussianProcessHandler & gp_utils, void * context)
{
  dataStore(stream, gp_utils.hyperparamMap(), context);
  dataStore(stream, gp_utils.hyperparamVectorMap(), context);
  dataStore(stream, gp_utils.covarType(), context);
  dataStore(stream, gp_utils.K(), context);
  dataStore(stream, gp_utils.KResultsSolve(), context);
  dataStore(stream, gp_utils.KCholeskyDecomp(), context);
  dataStore(stream, gp_utils.paramStandardizer(), context);
  dataStore(stream, gp_utils.dataStandardizer(), context);
}

template <>
void
dataLoad(std::istream & stream, StochasticTools::GaussianProcessHandler & gp_utils, void * context)
{
  dataLoad(stream, gp_utils.hyperparamMap(), context);
  dataLoad(stream, gp_utils.hyperparamVectorMap(), context);
  dataLoad(stream, gp_utils.covarType(), context);
  dataLoad(stream, gp_utils.K(), context);
  dataLoad(stream, gp_utils.KResultsSolve(), context);
  dataLoad(stream, gp_utils.KCholeskyDecomp(), context);
  dataLoad(stream, gp_utils.paramStandardizer(), context);
  dataLoad(stream, gp_utils.dataStandardizer(), context);
}
