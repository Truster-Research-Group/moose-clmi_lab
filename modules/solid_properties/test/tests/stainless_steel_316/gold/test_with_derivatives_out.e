CDF      
      
len_string     !   len_line   Q   four      	time_step          len_name   !   num_dim       	num_nodes      	   num_elem      
num_el_blk        num_node_sets         num_side_sets         num_el_in_blk1        num_nod_per_el1       num_side_ss1      num_side_ss2      num_side_ss3      num_side_ss4      num_nod_ns1       num_nod_ns2       num_nod_ns3       num_nod_ns4       num_nod_var       num_elem_var      num_info  „         api_version       @§
=   version       @§
=   floating_point_word_size            	file_size               int64_status             title         test_with_derivatives_out.e    maximum_name_length                 (   
time_whole                            l   	eb_status                             
ä   eb_prop1               name      ID              
è   	ns_status         	                    
ì   ns_prop1      	         name      ID              
ü   	ss_status         
                       ss_prop1      
         name      ID                 coordx                      H      ,   coordy                      H      t   eb_names                       $      Œ   ns_names      	                       à   ss_names      
                       d   
coor_names                         D      è   node_num_map                    $      ,   connect1                  	elem_type         QUAD4         @      P   elem_num_map                             elem_ss1                              side_ss1                          š   elem_ss2                          °   side_ss2                          ž   elem_ss3                          À   side_ss3                          È   elem_ss4                          Đ   side_ss4                          Ű   node_ns1                          à   node_ns2                          ì   node_ns3                          ű   node_ns4                             vals_nod_var1                          H      t   vals_nod_var2                          H      Œ   name_nod_var                       D         name_elem_var                          È      T   vals_elem_var1eb1                                    vals_elem_var2eb1                                 $   vals_elem_var3eb1                                 D   vals_elem_var4eb1                                 d   vals_elem_var5eb1                                    vals_elem_var6eb1                                 €   elem_var_tab                                info_records                      8      4                                                                 ?à      ?à              ?đ      ?đ      ?à              ?đ                      ?à      ?à              ?à      ?đ      ?đ      ?đ                                          bottom                           left                             right                            top                              bottom                           left                             top                              right                                                                                                                           	                                             	                                                                                          	         	T                                u                                  cp @@     @@     @@     @@  k  @@     @@     @@     @@   rho @                            dcp_dT                           dk_dT                            drho_dT                                              ####################@@     @@     @@     @@     @@     @@     @@     @@  # Created by MOOSE #                                                             ####################                                                             ### Command Line Arguments ###                                                    -i test_with_derivatives.i### Version Info ###                                  Framework Information:                                                           MOOSE Version:           git commit 8aa9c0d on 2018-11-30                        LibMesh Version:         ab2cf97250f90e88b1da3c9fb40931cf46af7ba9                PETSc Version:           3.8.3                                                   Current Time:            Fri Nov 30 10:51:50 2018                                Executable Timestamp:    Fri Nov 30 10:51:49 2018                                                                                                                                                                                                  ### Input File ###                                                                                                                                                []                                                                                 inactive                       =                                                 element_order                  = AUTO                                            order                          = AUTO                                            side_order                     = AUTO                                            type                           = GAUSS                                         []                                                                                                                                                                [AuxKernels]                                                                                                                                                        [./cp]                                                                             inactive                     =                                                   isObjectAction               = 1                                                 type                         = MaterialRealAux                                   block                        = INVALID                                           boundary                     = INVALID                                           control_tags                 = AuxKernels                                        enable                       = 1                                                 execute_on                   = 'LINEAR TIMESTEP_END'                             factor                       = 1                                                 offset                       = 0                                                 property                     = cp_solid                                          seed                         = 0                                                 use_displaced_mesh           = 0                                                 variable                     = cp                                              [../]                                                                                                                                                             [./dcp_dT]                                                                         inactive                     =                                                   isObjectAction               = 1                                                 type                         = MaterialRealAux                                   block                        = INVALID                                           boundary                     = INVALID                                           control_tags                 = AuxKernels                                        enable                       = 1                                                 execute_on                   = 'LINEAR TIMESTEP_END'                             factor                       = 1                                                 offset                       = 0                                                 property                     = dcp_solid/dT                                      seed                         = 0                                                 use_displaced_mesh           = 0                                                 variable                     = dcp_dT                                          [../]                                                                                                                                                             [./dk_dT]                                                                          inactive                     =                                                   isObjectAction               = 1                                                 type                         = MaterialRealAux                                   block                        = INVALID                                           boundary                     = INVALID                                           control_tags                 = AuxKernels                                        enable                       = 1                                                 execute_on                   = 'LINEAR TIMESTEP_END'                             factor                       = 1                                                 offset                       = 0                                                 property                     = dk_solid/dT                                       seed                         = 0                                                 use_displaced_mesh           = 0                                                 variable                     = dk_dT                                           [../]                                                                                                                                                             [./drho_dT]                                                                        inactive                     =                                                   isObjectAction               = 1                                                 type                         = MaterialRealAux                                   block                        = INVALID                                           boundary                     = INVALID                                           control_tags                 = AuxKernels                                        enable                       = 1                                                 execute_on                   = 'LINEAR TIMESTEP_END'                             factor                       = 1                                                 offset                       = 0                                                 property                     = drho_solid/dT                                     seed                         = 0                                                 use_displaced_mesh           = 0                                                 variable                     = drho_dT                                         [../]                                                                                                                                                             [./k]                                                                              inactive                     =                                                   isObjectAction               = 1                                                 type                         = MaterialRealAux                                   block                        = INVALID                                           boundary                     = INVALID                                           control_tags                 = AuxKernels                                        enable                       = 1                                                 execute_on                   = 'LINEAR TIMESTEP_END'                             factor                       = 1                                                 offset                       = 0                                                 property                     = k_solid                                           seed                         = 0                                                 use_displaced_mesh           = 0                                                 variable                     = k                                               [../]                                                                                                                                                             [./rho]                                                                            inactive                     =                                                   isObjectAction               = 1                                                 type                         = MaterialRealAux                                   block                        = INVALID                                           boundary                     = INVALID                                           control_tags                 = AuxKernels                                        enable                       = 1                                                 execute_on                   = 'LINEAR TIMESTEP_END'                             factor                       = 1                                                 offset                       = 0                                                 property                     = rho_solid                                         seed                         = 0                                                 use_displaced_mesh           = 0                                                 variable                     = rho                                             [../]                                                                          []                                                                                                                                                                [AuxVariables]                                                                                                                                                      [./T]                                                                              block                        = INVALID                                           family                       = LAGRANGE                                          inactive                     =                                                   initial_condition            = 1273.15                                           order                        = FIRST                                             outputs                      = INVALID                                           initial_from_file_timestep   = LATEST                                            initial_from_file_var        = INVALID                                         [../]                                                                                                                                                             [./cp]                                                                             block                        = INVALID                                           family                       = MONOMIAL                                          inactive                     =                                                   initial_condition            = INVALID                                           order                        = CONSTANT                                          outputs                      = INVALID                                           initial_from_file_timestep   = LATEST                                            initial_from_file_var        = INVALID                                         [../]                                                                                                                                                             [./dcp_dT]                                                                         block                        = INVALID                                           family                       = MONOMIAL                                          inactive                     =                                                   initial_condition            = INVALID                                           order                        = CONSTANT                                          outputs                      = INVALID                                           initial_from_file_timestep   = LATEST                                            initial_from_file_var        = INVALID                                         [../]                                                                                                                                                             [./dk_dT]                                                                          block                        = INVALID                                           family                       = MONOMIAL                                          inactive                     =                                                   initial_condition            = INVALID                                           order                        = CONSTANT                                          outputs                      = INVALID                                           initial_from_file_timestep   = LATEST                                            initial_from_file_var        = INVALID                                         [../]                                                                                                                                                             [./drho_dT]                                                                        block                        = INVALID                                           family                       = MONOMIAL                                          inactive                     =                                                   initial_condition            = INVALID                                           order                        = CONSTANT                                          outputs                      = INVALID                                           initial_from_file_timestep   = LATEST                                            initial_from_file_var        = INVALID                                         [../]                                                                                                                                                             [./k]                                                                              block                        = INVALID                                           family                       = MONOMIAL                                          inactive                     =                                                   initial_condition            = INVALID                                           order                        = CONSTANT                                          outputs                      = INVALID                                           initial_from_file_timestep   = LATEST                                            initial_from_file_var        = INVALID                                         [../]                                                                                                                                                             [./rho]                                                                            block                        = INVALID                                           family                       = MONOMIAL                                          inactive                     =                                                   initial_condition            = INVALID                                           order                        = CONSTANT                                          outputs                      = INVALID                                           initial_from_file_timestep   = LATEST                                            initial_from_file_var        = INVALID                                         [../]                                                                          []                                                                                                                                                                [BCs]                                                                                                                                                               [./left]                                                                           boundary                     = left                                              control_tags                 = INVALID                                           enable                       = 1                                                 extra_matrix_tags            = INVALID                                           extra_vector_tags            = INVALID                                           implicit                     = 1                                                 inactive                     =                                                   isObjectAction               = 1                                                 matrix_tags                  = system                                            type                         = DirichletBC                                       use_displaced_mesh           = 0                                                 variable                     = u                                                 vector_tags                  = nontime                                           diag_save_in                 = INVALID                                           save_in                      = INVALID                                           seed                         = 0                                                 value                        = 1000                                            [../]                                                                                                                                                             [./right]                                                                          boundary                     = right                                             control_tags                 = INVALID                                           enable                       = 1                                                 extra_matrix_tags            = INVALID                                           extra_vector_tags            = INVALID                                           implicit                     = 1                                                 inactive                     =                                                   isObjectAction               = 1                                                 matrix_tags                  = system                                            type                         = DirichletBC                                       use_displaced_mesh           = 0                                                 variable                     = u                                                 vector_tags                  = nontime                                           diag_save_in                 = INVALID                                           save_in                      = INVALID                                           seed                         = 0                                                 value                        = 500                                             [../]                                                                          []                                                                                                                                                                [Executioner]                                                                      inactive                       =                                                 isObjectAction                 = 1                                               type                           = Steady                                          compute_initial_residual_before_preset_bcs = 0                                   contact_line_search_allowed_lambda_cuts = 2                                      contact_line_search_ltol       = INVALID                                         control_tags                   =                                                 enable                         = 1                                               l_abs_step_tol                 = -1                                              l_max_its                      = 10000                                           l_tol                          = 1e-05                                           line_search                    = default                                         line_search_package            = petsc                                           mffd_type                      = wp                                              nl_abs_step_tol                = 1e-50                                           nl_abs_tol                     = 1e-50                                           nl_max_funcs                   = 10000                                           nl_max_its                     = 50                                              nl_rel_step_tol                = 1e-50                                           nl_rel_tol                     = 1e-08                                           no_fe_reinit                   = 0                                               petsc_options                  = INVALID                                         petsc_options_iname            = INVALID                                         petsc_options_value            = INVALID                                         restart_file_base              =                                                 snesmf_reuse_base              = 1                                               solve_type                     = INVALID                                         splitting                      = INVALID                                       []                                                                                                                                                                [Kernels]                                                                                                                                                           [./diff]                                                                           inactive                     =                                                   isObjectAction               = 1                                                 type                         = Diffusion                                         block                        = INVALID                                           control_tags                 = Kernels                                           diag_save_in                 = INVALID                                           enable                       = 1                                                 extra_matrix_tags            = INVALID                                           extra_vector_tags            = INVALID                                           implicit                     = 1                                                 matrix_tags                  = system                                            save_in                      = INVALID                                           seed                         = 0                                                 use_displaced_mesh           = 0                                                 variable                     = u                                                 vector_tags                  = nontime                                         [../]                                                                          []                                                                                                                                                                [Materials]                                                                                                                                                         [./sp_mat]                                                                         inactive                     =                                                   isObjectAction               = 1                                                 type                         = ThermalStainlessSteel316Properties                block                        = INVALID                                           boundary                     = INVALID                                           compute                      = 1                                                 constant_on                  = NONE                                              control_tags                 = Materials                                         emissivity                   = 1                                                 enable                       = 1                                                 implicit                     = 1                                                 output_properties            = INVALID                                           outputs                      = none                                              seed                         = 0                                                 surface                      = oxidized                                          temperature                  = T                                                 use_displaced_mesh           = 0                                               [../]                                                                          []                                                                                                                                                                [Mesh]                                                                             inactive                       =                                                 displacements                  = INVALID                                         block_id                       = INVALID                                         block_name                     = INVALID                                         boundary_id                    = INVALID                                         boundary_name                  = INVALID                                         construct_side_list_from_node_list = 0                                           ghosted_boundaries             = INVALID                                         ghosted_boundaries_inflation   = INVALID                                         isObjectAction                 = 1                                               second_order                   = 0                                               skip_partitioning              = 0                                               type                           = GeneratedMesh                                   uniform_refine                 = 0                                               allow_renumbering              = 1                                               bias_x                         = 1                                               bias_y                         = 1                                               bias_z                         = 1                                               centroid_partitioner_direction = INVALID                                         construct_node_list_from_side_list = 1                                           control_tags                   =                                                 dim                            = 2                                               elem_type                      = INVALID                                         enable                         = 1                                               gauss_lobatto_grid             = 0                                               ghosting_patch_size            = INVALID                                         max_leaf_size                  = 10                                              nemesis                        = 0                                               nx                             = 2                                               ny                             = 2                                               nz                             = 1                                               parallel_type                  = DEFAULT                                         partitioner                    = default                                         patch_size                     = 40                                              patch_update_strategy          = never                                           xmax                           = 1                                               xmin                           = 0                                               ymax                           = 1                                               ymin                           = 0                                               zmax                           = 1                                               zmin                           = 0                                             []                                                                                                                                                                [Mesh]                                                                           []                                                                                                                                                                [Outputs]                                                                          append_date                    = 0                                               append_date_format             = INVALID                                         checkpoint                     = 0                                               color                          = 1                                               console                        = 1                                               controls                       = 0                                               csv                            = 0                                               dofmap                         = 0                                               execute_on                     = 'INITIAL TIMESTEP_END'                          exodus                         = 1                                               file_base                      = INVALID                                         gmv                            = 0                                               gnuplot                        = 0                                               hide                           = INVALID                                         inactive                       =                                                 interval                       = 1                                               nemesis                        = 0                                               output_if_base_contains        = INVALID                                         perf_graph                     = 0                                               print_linear_residuals         = 1                                               print_mesh_changed_info        = 0                                               print_perf_log                 = 0                                               show                           = INVALID                                         solution_history               = 0                                               sync_times                     =                                                 tecplot                        = 0                                               vtk                            = 0                                               xda                            = 0                                               xdr                            = 0                                             []                                                                                                                                                                [Variables]                                                                                                                                                         [./u]                                                                              block                        = INVALID                                           eigen                        = 0                                                 family                       = LAGRANGE                                          inactive                     =                                                   initial_condition            = 1000                                              order                        = FIRST                                             outputs                      = INVALID                                           scaling                      = 1                                                 initial_from_file_timestep   = LATEST                                            initial_from_file_var        = INVALID                                         [../]                                                                          []                                                                                          @ä@ä@ä@ä@ä@ä@ä@ä@ä@@     @@     @@     @@     @@     @@     @@     @@     @@                                                                                                                                                                                                     ?đ      @ä@ä@ä@ä@ä@ä@ä@ä@ä@@     @oÿÿÿÿü@oÿÿÿÿĐ@@     @?ÿÿÿÿÿ@?ÿÿÿÿÿ@oÿÿÿÿȚ@@     @?ÿÿÿÿÿ@Oô4ă@Oô4ă@Oô4ă@Oô4ă@=o@=o@=o@=o@œ.!Ï@œ.!Ï@œ.!Ï@œ.!Ï?Ç>«6z?Ç>«6z?Ç>«6z?Ç>«6z?ĘòŻą~?ĘòŻą~?ĘòŻą~?ĘòŻą~żáa,Ś4Àăżáa,Ś4Àăżáa,Ś4Àăżáa,Ś4Àă