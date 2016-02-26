#ifdef SYMGS_LEVEL
#include "LevelSYMGS.hpp"

#ifdef KOKKOS_TEAM
typedef Kokkos::TeamPolicy<>              team_policy ;
typedef team_policy::member_type team_member ;
class leveledForwardSweep{
  public:
  local_int_t f_lev_start;
  local_int_1d_type f_lev_ind;

  local_matrix_type A;
  double_1d_type rv, zv;
  int_1d_type matrixDiagonal;

  int localNumberOfRows;
  int rpt; // = rows_per_team;

  leveledForwardSweep(const local_int_t f_lev_start_, const local_int_1d_type & f_lev_ind_, 
    const local_matrix_type& A_, const double_1d_type& rv_, double_1d_type& zv_,
    const int_1d_type& matrixDiagonal_, const int localNumberOfRows_, const int rpt_):
    f_lev_start(f_lev_start_), f_lev_ind(f_lev_ind_), A(A_), rv(rv_), zv(zv_),
    matrixDiagonal(matrixDiagonal_), localNumberOfRows(localNumberOfRows_), rpt(rpt_) {}

  KOKKOS_INLINE_FUNCTION
  void operator()(const team_member & thread) const{
    // Need to find a way to assign rows to a team.
    // NEEDS:
    // Number of rows in level.
    // Number of rows per team. This should probably vary between levels
    // Assign each thread in the team a starting point in f_lev_ind based on the above.
    //  Make sure no thread exceeds the level we are in. (Simple if check will do).
    // Peform the operations in a parallel for over their set of rows.
    const int row_idx = thread.league_rank() * rpt + f_lev_start; // Figure out where in f_lev_ind we are.
                const int end_row= row_idx+rpt>localNumberOfRows?localNumberOfRows:row_idx+rpt;
    // BE SURE THAT WHEN CALLING THIS FUNCTOR THAT OUR ROWS DIVIDE EVENLY AMONG THE TEAMS. 
    // May be difficult if the number of rows in the level is prime or really small.
    
    Kokkos::parallel_for(Kokkos::TeamThreadRange(thread, row_idx, end_row), [=](int& irow){
      // This makes our team run parallel over all the rows i where row_idx <= i < row_idx+rpt
      //assert(irow >= row_idx && irow < row_idx+rpt);
      local_int_t currentRow = f_lev_ind(irow); // I think that irow is between row_idx and row_idx+rpt
      const int start = A.graph.row_map(currentRow);
      const int diagIdx = matrixDiagonal(currentRow);
      const int vector_range = diagIdx - start;
      double sum =0; // rv(currentRow);
      Kokkos::parallel_reduce(Kokkos::ThreadVectorRange(thread, vector_range),
          [&](const int& li, double& lsum){
            const int i = li + start;
            lsum += -zv(A.graph.entries(i))*A.values(i);
          }, sum);
  
                         sum=sum+ rv(currentRow);
      //RUN A SERIAL FOR LOOP HERE
      //for(int li = 0; li<vector_range; li++){
                          //      const int i=li+start;
      //  sum -= zv(A.graph.entries(i))*A.values(i);}
      //TRY KOKKOS SINGLE HERE
                        Kokkos::single(Kokkos::PerThread(thread),[=] () {
              zv(currentRow) = sum / A.values(diagIdx);
                        });
    });
  }
};

class leveledBackSweep{
  public:
    local_int_t b_lev_start;
    local_int_1d_type b_lev_ind;

    local_matrix_type A;
    double_1d_type zv, xv;
    int_1d_type matrixDiagonal;

    int localNumberOfRows;
    int rpt; // = rows_per_team;

  leveledBackSweep(const local_int_t b_lev_start_, const local_int_1d_type & b_lev_ind_, 
    const local_matrix_type& A_, const double_1d_type& zv_, double_1d_type& xv_,
    const int_1d_type& matrixDiagonal_, const int localNumberOfRows_, const int rpt_):
    b_lev_start(b_lev_start_), b_lev_ind(b_lev_ind_), A(A_), zv(zv_), xv(xv_),
    matrixDiagonal(matrixDiagonal_), localNumberOfRows(localNumberOfRows_), rpt(rpt_) {}

  KOKKOS_INLINE_FUNCTION
  void operator()(const team_member & thread) const{
    const int row_idx = thread.league_rank() * rpt + b_lev_start;
    const int end_row = row_idx + rpt > localNumberOfRows?localNumberOfRows:row_idx + rpt;

    Kokkos::parallel_for(Kokkos::TeamThreadRange(thread, row_idx, end_row), [=](int& irow){
      local_int_t currentRow = b_lev_ind(irow);
      const int diagIdx = matrixDiagonal(currentRow);
      const int start = diagIdx + 1;
      const int end = A.graph.row_map(currentRow+1);
      const int vector_range = end - start;
      double sum = 0;
      Kokkos::parallel_reduce(Kokkos::ThreadVectorRange(thread, vector_range),
        [&](const int& li, double& lsum){
          const int i = li +start;
          lsum += -xv(A.graph.entries(i))*A.values(i);
        }, sum);
      sum += zv(currentRow);
      Kokkos::single(Kokkos::PerThread(thread), [=] () {
        xv(currentRow) = sum / A.values(diagIdx);
      });
    });
  }
};

class applyD{
  public:
  local_matrix_type A;
  const_int_1d_type diag;
  double_1d_type z;

  applyD(const local_matrix_type& A_, const const_int_1d_type& diag_, const double_1d_type& z_):
    A(A_), diag(diag_), z(z_){}

  KOKKOS_INLINE_FUNCTION
  void operator()(const int & i)const{
    z(i) = z(i)*A.values(diag(i));
  }
};
#else
class leveledForwardSweep{
  public:
  local_int_t f_lev_start;
  local_int_1d_type f_lev_ind;

  local_matrix_type A;
  double_1d_type rv, zv;
  int_1d_type matrixDiagonal;

  leveledForwardSweep(const local_int_t f_lev_start_, const local_int_1d_type & f_lev_ind_, 
    const local_matrix_type& A_, const double_1d_type& rv_, double_1d_type& zv_,
    const int_1d_type& matrixDiagonal_):
    f_lev_start(f_lev_start_), f_lev_ind(f_lev_ind_), A(A_), rv(rv_), zv(zv_),
    matrixDiagonal(matrixDiagonal_){}

  KOKKOS_INLINE_FUNCTION
  void operator()(const int & i)const{
    local_int_t currentRow = f_lev_ind(f_lev_start+i);
    int start = A.graph.row_map(currentRow);
    const int diagIdx = matrixDiagonal(currentRow);
    double sum = rv(currentRow);
    for(int j = start; j < diagIdx; j++)
      sum -= zv(A.graph.entries(j))*A.values(j);
    zv(currentRow) = sum/A.values(diagIdx);
  }
};

class applyD{
  public:
  local_matrix_type A;
  const_int_1d_type diag;
  double_1d_type z;

  applyD(const local_matrix_type& A_, const const_int_1d_type& diag_, const double_1d_type& z_):
    A(A_), diag(diag_), z(z_){}

  KOKKOS_INLINE_FUNCTION
  void operator()(const int & i)const{
    z(i) = z(i)*A.values(diag(i));
  }
};

class leveledBackSweep{
  public:
  local_int_t b_lev_start;
  local_int_1d_type b_lev_ind;

  local_matrix_type A;
  double_1d_type zv, xv;
  int_1d_type matrixDiagonal;

  leveledBackSweep(const local_int_t b_lev_start_, const local_int_1d_type & b_lev_ind_, 
    const local_matrix_type& A_, const double_1d_type& zv_, double_1d_type& xv_,
    const int_1d_type& matrixDiagonal_):
    b_lev_start(b_lev_start_), b_lev_ind(b_lev_ind_), A(A_), zv(zv_), xv(xv_),
    matrixDiagonal(matrixDiagonal_){}

  KOKKOS_INLINE_FUNCTION
  void operator()(const int & i)const{
    local_int_t currentRow = b_lev_ind(b_lev_start+i);
    int end = A.graph.row_map(currentRow+1);
    const int diagIdx = matrixDiagonal(currentRow);
    double sum = zv(currentRow);
    for(int j = diagIdx+1; j < end; j++)
      sum -= xv(A.graph.entries(j))*A.values(j);
    xv(currentRow) = sum/A.values(diagIdx);
  }
};
#endif



int LevelSYMGS( const SparseMatrix & A, const Vector & r, Vector & x){
assert(x.localLength == A.localNumberOfColumns); // Make sure x contains space for halo values

#ifndef HPCG_NO_MPI
  ExchangeHalo(A,x);
#endif
  Optimatrix * A_Optimized = (Optimatrix*) A.optimizationData;
  local_matrix_type localMatrix = A_Optimized->localMatrix;
  local_int_1d_type matrixDiagonal = A_Optimized->matrixDiagonal;
  LevelScheduler * levels = A_Optimized->levels;
  const int f_numLevels = levels->f_numberOfLevels;
  const int b_numLevels = levels->b_numberOfLevels;
  host_local_int_1d_type f_lev_map = levels->f_lev_map;
  local_int_1d_type f_lev_ind = levels->f_lev_ind;
  host_local_int_1d_type b_lev_map = levels->b_lev_map;
  local_int_1d_type b_lev_ind = levels->b_lev_ind;
  local_int_1d_type f_row_level = levels->f_row_level;
  local_int_1d_type b_row_level = levels->b_row_level;

  Optivector * r_Optimized = (Optivector *) r.optimizationData;
  double_1d_type r_values = r_Optimized->values;

  Optivector * x_Optimized = (Optivector *) x.optimizationData;
  double_1d_type x_values = x_Optimized->values;

  double_1d_type z("z", x_values.dimension_0());
#ifdef KOKKOS_TEAM
  const int row_per_team=256;
  const int vector_size = 32;
  const int team_size_max = 32;
  for(int i = 0; i < f_numLevels; i++){  
    const int numrows = f_lev_map(i+1) - f_lev_map(i);
    const int numTeams = (numrows+row_per_team-1)/row_per_team;
    const team_policy policy(numTeams, team_size_max, vector_size);
    Kokkos::parallel_for(policy, leveledForwardSweep(f_lev_map(i), f_lev_ind, 
      localMatrix, r_values, z, matrixDiagonal, A.localNumberOfRows, row_per_team));
  }
  Kokkos::parallel_for(z.dimension_0(), applyD(localMatrix, matrixDiagonal, z));
  for(int i = 0; i < b_numLevels; i++){
    const int numrows = b_lev_map(i+1) - b_lev_map(i);
    const int numTeams = (numrows+row_per_team-1)/row_per_team;
    const team_policy policy(numTeams, team_size_max, vector_size);
    Kokkos::parallel_for(policy, leveledBackSweep(b_lev_map(i), b_lev_ind,
      localMatrix, z, x_values, matrixDiagonal, A.localNumberOfRows, row_per_team));
  }

  
#else
  for(int i = 0; i < f_numLevels; i++){
    int start = f_lev_map(i);
    int end = f_lev_map(i+1);
    Kokkos::parallel_for(end - start, leveledForwardSweep(start, f_lev_ind, localMatrix, r_values, z, matrixDiagonal));
  }
  Kokkos::parallel_for(z.dimension_0(), applyD(localMatrix, matrixDiagonal, z));
  for(int i = 0; i < b_numLevels; i++){
    int start = b_lev_map(i);
    int end = b_lev_map(i+1);
    Kokkos::parallel_for(end - start, leveledBackSweep(start, b_lev_ind, localMatrix, z, x_values, matrixDiagonal));
  }
#endif

  return (0);
}
#endif
