[GlobalParams]
  gravity_vector = '0 0 0'

  initial_T = 507
  initial_p = 6.e6
  initial_vel = 0.1
  initial_vel_x = 0.1
  initial_vel_y = 0.1
  initial_vel_z = 0.1

  closures = simple
[]

[FluidProperties]
  [./eos]
    type = StiffenedGasFluidProperties
    gamma = 2.35
    q = -1167e3
    q_prime = 0
    p_inf = 1.e9
    cv = 1816
  [../]
[]

[HeatStructureMaterials]
  [./mat1]
    type = SolidMaterialProperties
    k = 16
    Cp = 356.
    rho = 6.551400E+03
  [../]
[]

[Components]
  [./pipe1]
    type = FlowChannel1Phase
    fp = eos
    # geometry
    position = '0 0 0'
    orientation = '1 0 0'
    length = 1
    n_elems = 100
    A = 1.907720E-04
    D_h = 1.698566E-02
    f = 0.1
  [../]

  [./junction]
    type = VolumeJunction1Phase
    connections = 'pipe1:out pipe2:in'
    position = '1 0 0'
    volume = 1e-5
  [../]

  [./pipe2]
    type = FlowChannel1Phase
    fp = eos
    # geometry
    position = '1 0 0'
    orientation = '1 0 0'
    length = 1
    n_elems = 100
    A = 1.907720E-04
    D_h = 1.698566E-02
    f = 0.1
  [../]

  [./hs]
    type = HeatStructureCylindrical
    position = '1 0.01 0'
    orientation = '1 0 0'
    length = 1
    n_elems = 100
    names = '0'
    n_part_elems = 1
    materials = 'mat1'
    widths = 0.1
    initial_T = 508
  [../]

  [./inlet]
    type = InletDensityVelocity1Phase
    input = 'pipe1:in'
    rho = 824.8692479578525
    vel = 0.1
  [../]
  [./outlet]
    type = Outlet1Phase
    input = 'pipe2:out'
    p = 6e6
  [../]
[]

[Preconditioning]
  [./SMP_PJFNK]
    type = SMP
    full = true
  [../]
[]

[Problem]
  restart_file_base = part1_checkpoint_cp/0005
[]

[Executioner]
  type = Transient
  scheme = 'bdf2'

  dt = 1e-5
  num_steps = 10
  abort_on_solve_fail = true

  solve_type = 'PJFNK'
  line_search = 'basic'
  nl_rel_tol = 1e-8
  nl_abs_tol = 1e-8
  nl_max_its = 10

  l_tol = 1e-3
  l_max_its = 100

  automatic_scaling = true
[]

[Outputs]
  exodus = true
[]
