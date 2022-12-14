[Mesh]
  type = FileMesh
  file = rectangle.e
  dim = 2
  # This test can only be run with renumering disabled, so the
  # NodalVariableValue postprocessor's node id is well-defined.
  allow_renumbering = false
[]

[Variables]
  [./u]
    block = '1 2'
  [../]
  [./v]
    block = 2
  [../]
[]

[Kernels]
  [./diff]
    type = BlkResTestDiffusion
    variable = u
    block = '1 2'
  [../]
  [./v_diff]
    type = Diffusion
    variable = v
    block = 2
  [../]
[]

[BCs]
  [./left]
    type = DirichletBC
    variable = u
    boundary = 1
    value = 0
  [../]
  [./right]
    type = DirichletBC
    variable = u
    boundary = 2
    value = 1
  [../]
[]

[Materials]
  [./mat0]
    type = GenericConstantMaterial
    block = 1
    prop_names = 'a b'
    prop_values = '1 2'
  [../]
  [./mat1]
    type = GenericConstantMaterial
    block = 2
    prop_names = a
    prop_values = 10
  [../]
[]

[Postprocessors]
  [./off_block]
    type = NodalVariableValue
    variable = v
    nodeid = 0
  [../]
[]

[Executioner]
  type = Steady
  solve_type = PJFNK
  petsc_options_iname = '-pc_type -pc_hypre_type'
  petsc_options_value = 'hypre boomeramg'
[]
