//=====================================================
// Copyright (C) 2008 Gael Guennebaud <gael.guennebaud@inria.fr>
//=====================================================
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
#include "action_hessenberg.hh"
#include "action_partial_lu.hh"
#include "basic_actions.hh"
#include "bench.hh"
#include "gmm_interface.hh"
#include "utilities.h"

BTL_MAIN;

int
main()
{

	bench<Action_axpy<gmm_interface<REAL_TYPE>>>(MIN_AXPY, MAX_AXPY, NB_POINT);
	bench<Action_axpby<gmm_interface<REAL_TYPE>>>(MIN_AXPY, MAX_AXPY, NB_POINT);

	bench<Action_matrix_vector_product<gmm_interface<REAL_TYPE>>>(MIN_MV, MAX_MV, NB_POINT);
	bench<Action_atv_product<gmm_interface<REAL_TYPE>>>(MIN_MV, MAX_MV, NB_POINT);

	bench<Action_matrix_matrix_product<gmm_interface<REAL_TYPE>>>(MIN_MM, MAX_MM, NB_POINT);
	//   bench<Action_ata_product<gmm_interface<REAL_TYPE> > >(MIN_MM,MAX_MM,NB_POINT);
	//   bench<Action_aat_product<gmm_interface<REAL_TYPE> > >(MIN_MM,MAX_MM,NB_POINT);

	bench<Action_trisolve<gmm_interface<REAL_TYPE>>>(MIN_MM, MAX_MM, NB_POINT);
	// bench<Action_lu_solve<blitz_LU_solve_interface<REAL_TYPE> > >(MIN_LU,MAX_LU,NB_POINT);

	bench<Action_partial_lu<gmm_interface<REAL_TYPE>>>(MIN_MM, MAX_MM, NB_POINT);

	bench<Action_hessenberg<gmm_interface<REAL_TYPE>>>(MIN_MM, MAX_MM, NB_POINT);
	bench<Action_tridiagonalization<gmm_interface<REAL_TYPE>>>(MIN_MM, MAX_MM, NB_POINT);

	return 0;
}
