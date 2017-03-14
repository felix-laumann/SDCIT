//
// Created by Sanghack Lee on 3/14/17.
//
//
#include <random>
#include <thread>
#include <set>
#include <iostream>
#include "SDCIT.h"
#include "permutation.h"
//
using std::thread;
using std::vector;
using std::mt19937;


std::pair<vector<int>, vector<std::pair<int, int> >> perm_and_mask(const vector<double> &D_Z, const int n, const vector<int> sample, mt19937 generator) {
    vector<int> permutation;    // relative index
    std::set<std::pair<int, int> > setmask;  // relative index

    permutation = split_permutation(&D_Z[0], n, sample, generator);

    // mask to hide!
    const int sample_size = sample.size();
    for (int i = 0; i < sample_size; i++) {
        setmask.insert(std::make_pair(i, i));
        setmask.insert(std::make_pair(i, permutation[i]));
        setmask.insert(std::make_pair(permutation[i], i));
    }
    vector<std::pair<int, int> > mask(setmask.begin(), setmask.end());
    return std::make_pair(permutation, mask);
}


std::tuple<double, vector<int>, vector<std::pair<int, int> >> MMSD(const double *K_XZ, const double *K_Y, const vector<double> &D_Z, const int n, const vector<int> &sample, mt19937 generator) {
    double test_statistic = 0.0;
    vector<int> permutation;
    vector<std::pair<int, int> > mask;

    std::tie(permutation, mask) = perm_and_mask(D_Z, n, sample, generator);

    const int sample_size = sample.size();
    for (int i = 0; i < sample_size; i++) {
        for (int j = 0; j < sample_size; j++) {
            test_statistic += K_XZ[sample[i] * n + sample[j]] * (K_Y[sample[i] * n + sample[j]] + K_Y[sample[permutation[i]] * n + sample[permutation[j]]] - K_Y[sample[i] * n + sample[permutation[j]]]) -
                              K_XZ[sample[j] * n + sample[i]] * K_Y[sample[j] * n + sample[permutation[i]]];
        }
    }
    for (const auto &rc: mask) {
        const int i = rc.first;
        const int j = rc.second;
        test_statistic -= K_XZ[sample[i] * n + sample[j]] * (K_Y[sample[i] * n + sample[j]] + K_Y[sample[permutation[i]] * n + sample[permutation[j]]] - K_Y[sample[i] * n + sample[permutation[j]]]) -
                          K_XZ[sample[j] * n + sample[i]] * K_Y[sample[j] * n + sample[permutation[i]]];
    }
    test_statistic /= (sample_size * sample_size) - mask.size();

    return std::make_tuple(test_statistic, permutation, mask);
}


// Returns a copy of distance matrix with max value added.
vector<double> penalty_distance(const vector<double> &D_Z, const int n, vector<std::pair<int, int> > mask) {
//    const double inf = std::numeric_limits<double>::infinity();
    vector<double> copied_D_Z(D_Z);
    double max_val = *std::max_element(copied_D_Z.begin(), copied_D_Z.end());

    for (const auto &rc : mask) {
        copied_D_Z[rc.first * n + rc.second] += 2.0 * max_val;
    }
    return copied_D_Z;
}


void multi_mmsd(const double *K_XZ, const double *K_Y, const vector<double> &D_Z, const int n, const int sub_b, double *const subnull, const unsigned int subseed) {
    mt19937 generator(subseed);

    vector<int> samples(n);
    std::iota(std::begin(samples), std::end(samples), 0);

    for (int i = 0; i < sub_b; i++) {
        std::shuffle(samples.begin(), samples.end(), generator);
        const vector<int> half_sample = vector<int>(samples.begin(), samples.begin() + (n / 2));

        double mmsd;
        std::tie(mmsd, std::ignore, std::ignore) = MMSD(K_XZ, K_Y, D_Z, n, half_sample, generator);

        subnull[i] = mmsd;
    }
}


vector<double> shuffle_matrix(const double *mat, const int n, const vector<int> &perm) {
    vector<double> newmat;
    newmat.reserve(n * n);
    for (int i = 0; i < n; i++) {
        const int pin = perm[i] * n;
        const int in = i * n;
        for (int j = 0; j < n; j++) {
            newmat[in + j] = mat[pin + perm[j]];
        }
    }
    return newmat;
}


void c_sdcit(const double *K_XZ, const double *K_Y, const double *D_Z_, const int n,
             const int b, const int seed, const int n_threads,
             double *const mmsd, double *const null) {
    mt19937 generator(seed);

    double test_statistic;
    vector<int> permutation;
    vector<std::pair<int, int> > mask;
    const vector<double> D_Z(D_Z_, D_Z_ + (n * n));
    vector<int> full_idx(n);
    std::iota(std::begin(full_idx), std::end(full_idx), 0);

    std::tie(test_statistic, permutation, mask) = MMSD(K_XZ, K_Y, D_Z, n, full_idx, generator);

    std::tie(permutation, mask) = perm_and_mask(penalty_distance(D_Z, n, mask), n, full_idx, generator);
    const auto D_Z_for_null = penalty_distance(D_Z, n, mask);
    const auto K_Y_null = shuffle_matrix(K_Y, n, permutation);
    vector<thread> threads;
    int b_offset = 0;
    for (int i = 0; i < n_threads; i++) {
        const unsigned int sub_seed = generator();
        const int remnant_b = (i < (b % n_threads)) ? 1 : 0;
        const int sub_b = remnant_b + b / n_threads;

        threads.push_back(thread(multi_mmsd, K_XZ, &K_Y_null[0], D_Z_for_null, n, sub_b, null + b_offset, sub_seed));

        b_offset += sub_b;
    }
    for (int i = 0; i < n_threads; i++) { threads[i].join(); }

    *mmsd = test_statistic;
}