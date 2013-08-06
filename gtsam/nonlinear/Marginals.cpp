/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation, 
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file Marginals.cpp
 * @brief 
 * @author Richard Roberts
 * @date May 14, 2012
 */

#include <gtsam/base/timing.h>
#include <gtsam/linear/JacobianFactor.h>
#include <gtsam/linear/HessianFactor.h>
#include <gtsam/nonlinear/Marginals.h>

using namespace std;

namespace gtsam {

/* ************************************************************************* */
Marginals::Marginals(const NonlinearFactorGraph& graph, const Values& solution, Factorization factorization)
{
  gttic(MarginalsConstructor);

  // Linearize graph
  graph_ = *graph.linearize(solution);

  // Store values
  values_ = solution;

  // Compute BayesTree
  factorization_ = factorization;
  if(factorization_ == CHOLESKY)
    bayesTree_ = *graph_.eliminateMultifrontal(boost::none, EliminatePreferCholesky);
  else if(factorization_ == QR)
    bayesTree_ = *graph_.eliminateMultifrontal(boost::none, EliminateQR);
}

/* ************************************************************************* */
void Marginals::print(const std::string& str, const KeyFormatter& keyFormatter) const
{
  graph_.print(str+"Graph: ");
  values_.print(str+"Solution: ", keyFormatter);
  bayesTree_.print(str+"Bayes Tree: ");
}

/* ************************************************************************* */
Matrix Marginals::marginalCovariance(Key variable) const {
  return marginalInformation(variable).inverse();
}

/* ************************************************************************* */
Matrix Marginals::marginalInformation(Key variable) const {
  gttic(marginalInformation);

  // Compute marginal
  GaussianFactor::shared_ptr marginalFactor;
  if(factorization_ == CHOLESKY)
    marginalFactor = bayesTree_.marginalFactor(variable, EliminatePreferCholesky);
  else if(factorization_ == QR)
    marginalFactor = bayesTree_.marginalFactor(variable, EliminateQR);

  // Get information matrix (only store upper-right triangle)
  gttic(AsMatrix);
  return marginalFactor->information();
}

/* ************************************************************************* */
JointMarginal Marginals::jointMarginalCovariance(const std::vector<Key>& variables) const {
  JointMarginal info = jointMarginalInformation(variables);
  info.blockMatrix_.full() = info.blockMatrix_.full().inverse();
  return info;
}

/* ************************************************************************* */
JointMarginal Marginals::jointMarginalInformation(const std::vector<Key>& variables) const {

  // If 2 variables, we can use the BayesTree::joint function, otherwise we
  // have to use sequential elimination.
  if(variables.size() == 1)
  {
    Matrix info = marginalInformation(variables.front());
    std::vector<size_t> dims;
    dims.push_back(info.rows());
    return JointMarginal(info, dims, variables);
  }
  else
  {
    // Compute joint marginal factor graph.
    GaussianFactorGraph jointFG;
    if(variables.size() == 2) {
      if(factorization_ == CHOLESKY)
        jointFG = *bayesTree_.joint(variables[0], variables[1], EliminatePreferCholesky);
      else if(factorization_ == QR)
        jointFG = *bayesTree_.joint(variables[0], variables[1], EliminateQR);
    } else {
      if(factorization_ == CHOLESKY)
        jointFG = GaussianFactorGraph(*graph_.marginalMultifrontalBayesTree(Ordering(variables), boost::none, EliminatePreferCholesky));
      else if(factorization_ == QR)
        jointFG = GaussianFactorGraph(*graph_.marginalMultifrontalBayesTree(Ordering(variables), boost::none, EliminateQR));
    }

    // Get dimensions from factor graph
    std::vector<size_t> dims;
    dims.reserve(variables.size());
    BOOST_FOREACH(Key key, variables) {
      dims.push_back(values_.at(key).dim());
    }

    // Get information matrix
    Matrix augmentedInfo = jointFG.augmentedHessian();
    Matrix info = augmentedInfo.topLeftCorner(augmentedInfo.rows()-1, augmentedInfo.cols()-1);

    return JointMarginal(info, dims, variables);
  }
}

/* ************************************************************************* */
void JointMarginal::print(const std::string& s, const KeyFormatter& formatter) const {
  cout << s << "Joint marginal on keys ";
  bool first = true;
  BOOST_FOREACH(Key key, keys_) {
    if(!first)
      cout << ", ";
    else
      first = false;
    cout << formatter(key);
  }
  cout << ".  Use 'at' or 'operator()' to query matrix blocks." << endl;
}

} /* namespace gtsam */
