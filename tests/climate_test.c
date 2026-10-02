#include "world/climate.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
/* Golden values from the Beta NoiseGenerator2/Octaves2 Java reference.
 * Covers negative seeds/coordinates and the 30 million block boundary. */
static const struct { uint64_t seed; int x,z; double temperature,humidity; } golden[]={
{UINT64_C(0),0,0,0.91868674455220250,0.51627697992666610},
{UINT64_C(0),15,15,0.95448028094007250,0.62657631141494560},
{UINT64_C(0),-17,31,0.97010301369125430,0.56081582543047990},
{UINT64_C(0),512,-800,0.97440232293914310,0.75604236623797890},
{UINT64_C(0),30000000,-30000000,0.86918973533074030,0.86794942059511160},
{UINT64_C(42),0,0,0.97749264895809070,1.0000000000000000},
{UINT64_C(42),15,15,0.94321176261486350,1.0000000000000000},
{UINT64_C(42),-17,31,0.99188711668229020,1.0000000000000000},
{UINT64_C(42),512,-800,0.99296058014841070,0.34017626476693990},
{UINT64_C(42),30000000,-30000000,0.98809555298997800,0.82379395283585470},
{UINT64_MAX,0,0,0.91357931476844310,0.059566324824317050},
{UINT64_MAX,15,15,0.92197003786801480,0.22773256768887450},
{UINT64_MAX,-17,31,0.83018219615292570,0.22641990153785674},
{UINT64_MAX,512,-800,0.94935275588696320,0.68632284045294090},
{UINT64_MAX,30000000,-30000000,0.16444090731010685,0.22947173862937797},
{UINT64_C(123456789012345),0,0,0.99893929539676510,0.71473230413721490},
{UINT64_C(123456789012345),15,15,0.99850985342224230,0.87697354471987280},
{UINT64_C(123456789012345),-17,31,0.99878152498102690,0.66210825500913160},
{UINT64_C(123456789012345),512,-800,0.99861730974865730,0.96907806648151170},
{UINT64_C(123456789012345),30000000,-30000000,0.56934766784693950,0.52092227930410200},
};
int main(void) {
    unsigned i; BetaClimate climate;
    for(i=0;i<sizeof(golden)/sizeof(golden[0]);++i) {
        double t,h; beta_climate_init(&climate,golden[i].seed);
        beta_climate_sample(&climate,golden[i].x,golden[i].z,&t,&h);
        assert(fabs(t-golden[i].temperature)<1e-11 && fabs(h-golden[i].humidity)<1e-11);
    }
    assert(beta_climate_precipitation(0,0)==2);
    assert(beta_climate_precipitation(.45,.9)==2);
    assert(beta_climate_precipitation(1,0)==0);
    assert(beta_climate_precipitation(.8,.8)==1);
    puts("Beta climate matches Java at all 20 golden points."); return 0;
}
