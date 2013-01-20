/* 
 * File:   Basis.cpp
 * Author: sigve
 * 
 * Created on January 7, 2013, 4:22 PM
 */

#include "Basis.h"

//------------------------------------------------------------------------------ 

Basis::Basis() {
}

//------------------------------------------------------------------------------ 

Basis::Basis(Config *cfg) : cfg(cfg) {
    try {
        w               = cfg->lookup("systemSettings.w");
        dim             = cfg->lookup("systemSettings.dim");
        coordinateType  = cfg->lookup("systemSettings.coordinateType");
        shells          = cfg->lookup("systemSettings.shells");
        sIntegrator     = cfg->lookup("spatialIntegration.integrator");
        int basisType   = cfg->lookup("systemSettings.basisType");
    } catch (const SettingNotFoundException &nfex) {
        cerr << "Basis::Basis(Setting* systemSettings)::Error reading from 'systemSettings' object setting." << endl;
    }
    sqrtW = sqrt(w);

    switch(dim){
    case 1:
        wf = new HarmonicOscillator1d(cfg);
        break;
    case 2:
        wf = new HarmonicOscillator2d(cfg);
        break;
    }

#if DEBUG
    cout << "Basis::Basis(Setting* systemSettings)" << endl;
    cout << "w = " << w << endl;
    cout << "dim = " << dim << endl;
#endif
}

//------------------------------------------------------------------------------ 
void Basis::createBasis()
{

    switch(coordinateType){
    case CARTESIAN:
        createCartesianBasis();
        break;
    case POLAR:
        createPolarBasis();
        break;
    }
}
//------------------------------------------------------------------------------
void Basis::createCartesianBasis()
{
    // Generating all single particle states and energies
    vec state = zeros(4);

    // TODO: generalize to different quantum numbers.
    switch (dim) {
    case 1:
        for (int n = 0; n <= shells; n++) {
            for (int spin = 1; spin >= -1; spin -= 2) {
                vec quantumNumbers(1);
                quantumNumbers[0] = n;
                state(0) = states.size();
                state(1) = n;
                state(2) = 0;
                state(3) = spin;
                states.push_back(state);
            }
        }
        break;
    case 2:
        for (int s = 0; s <= shells; s++) {
            for (int nx = 0; nx <= s; nx++) {
                for (int ny = 0; ny <= s; ny++) {
                    if(nx + ny == s){
                        for (int spin = 1; spin >= -1; spin -= 2) {

                        state(0) = states.size();
                        state(1) = nx;
                        state(2) = ny;
                        state(3) = spin;
                        states.push_back(state);
                        }
                    }
                }
            }
        }
        break;
    }
    cout << states.size() << " orbitals created" << endl;
#if DEBUG
    cout << "void Basis::createBasis()" << endl;
    for (int i = 0; i < states.size(); i++) {
        for (int j = 0; j < states[0].n_elem; j++) {
            cout << states[i][j] << " ";
        }
        cout << endl;
    }
#endif
}
//------------------------------------------------------------------------------
void Basis::createPolarBasis()
{
    vec state(4);
    switch (dim) {
    case 1:
        cerr << "A polar basis in 1d does not make sense..." << endl;
        exit(1);
        break;
    case 2:
        for (int k = 0; k <= shells; k++) {
            for (int i = 0; i <= floor(k / 2); i++) {
                for (int l = -k; l <= k; l++) {
                    if ((2 * i + abs(l) + 1) == k) {

                        // Spin up
                        state(0) = states.size();
                        state(1) = i;
                        state(2) = l;
                        state(3) = 1;
                        states.push_back(state);

                        // Spin down
                        state(0) = states.size();
                        state(1) = i;
                        state(2) = l;
                        state(3) = -1;
                        states.push_back(state);
                    }
                }
            }
        }
        break;
    }
#if DEBUG
    cout << "void Basis::createPolarBasis()" << endl;
    for (int i = 0; i < states.size(); i++) {
        for (int j = 0; j < states[0].n_elem; j++) {
            cout << states[i][j] << " ";
        }
        cout << endl;
    }
#endif
}
//------------------------------------------------------------------------------ 

/**
 * Computes all the interaction-elements between all possible configurations 
 * of orbitals.
 */
void Basis::computeInteractionelements() {
    cout << "Computing interaction elements" << endl;

    double tolerance = 1e-6;
    SpatialIntegrator *I;

    switch (sIntegrator) {
    case MONTE_CARLO:
       I = new MonteCarloIntegrator(cfg);
        break;
    case GAUSS_LAGUERRE:
         I = new GaussLaguerreIntegrator(cfg, wf);
        break;
    case GAUSS_HERMITE:
         I = new GaussHermiteIntegrator(cfg);
        break;
    case INTERACTION_INTEGRATOR:
         I = new InteractonIntegrator(cfg);
        break;
    }

    int nStates = states.size();
    double E;

    vector<vec> interactionElements;
    vec interactionElement(5);

    for (int p = 0; p < nStates; p++) {
        for (int q = p + 1; q < nStates; q++) {
            for (int r = 0; r < nStates; r++) {
                for (int s = r + 1; s < nStates; s++) {
                    E = 0;

                    // Anti-symmetrized matrix elements
                    if (states[p][3] == states[r][3] && states[q][3] == states[s][3]) {
                        E += I->integrate(states[p], states[q], states[r], states[s]);
                    }

                    if (states[p][3] == states[s][3] && states[q][3] == states[r][3]) {
                        E -= I->integrate(states[p], states[q], states[s], states[r]);
                    }

                    if (abs(E) > tolerance) {
                        interactionElement << p << q << r << s << E;
                        interactionElements.push_back(interactionElement);
                    }
                }
            }
        }
    }
    cout << "Done " << endl;
    // Storing results in a matrix
    int nInteractionElements = interactionElements.size();
    intElements = zeros(nInteractionElements, 5);
    for (int i = 0; i < nInteractionElements; i++) {
        intElements.row(i) = trans(interactionElements[i]);
    }

#if DEBUG
    cout << "void Basis::computeInteractionelements()" << endl;
    cout << "InteractionELements = " << interactionElements.size() << endl;
#endif
}

//------------------------------------------------------------------------------ 

mat Basis::getInteractionElements() {
    return intElements;
}
//------------------------------------------------------------------------------ 

void Basis::computeSpsEnergies() {
    cout << "Computing orbital energies" << endl;
    int nStates = states.size();
    spsEnergies = zeros(nStates, 1);

    // Computing the one body operators
    for (int i = 0; i < states.size(); i++) {
        spsEnergies[i] = wf->getEnergy(states[i]);
    }
#if DEBUG
    cout << "void Basis::computeSpsEnergies()" << endl;
    cout << "spsEnergies = " << spsEnergies << endl;
#endif
}

//------------------------------------------------------------------------------ 

vec Basis::getSpsEnergies() {
    return spsEnergies;
}
//------------------------------------------------------------------------------ 

vector<vec> Basis::getStates(){
    return states;
}

//------------------------------------------------------------------------------ 

Basis::~Basis() {
}
