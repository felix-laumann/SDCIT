from joblib import Parallel
from joblib import delayed
from tqdm import tqdm

from experiments.testing_utils import read_chaotic, read_postnonlinear_noise, postnonlinear_noise_configs, chaotic_configs
from kcipt.sdcit import SDCIT
from kcipt.utils import *


def test_chaotic(independent, gamma, trial, N):
    np.random.seed(trial)

    kx, ky, kz, Dz = read_chaotic(independent, gamma, trial, N)

    mmd, pval = SDCIT(kx, ky, kz, Dz, size_of_null_sample=1000, reserve_perm=True)
    return independent, gamma, trial, N, mmd, pval


def test_postnonlinear(independent, noise, trial, N):
    np.random.seed(trial)

    kx, ky, kz, Dz = read_postnonlinear_noise(independent, noise, trial, N)

    mmd, pval = SDCIT(kx, ky, kz, Dz, size_of_null_sample=1000, reserve_perm=True)
    return independent, noise, trial, N, mmd, pval


if __name__ == '__main__':
    if True:
        for independent, N, gamma in tqdm(chaotic_configs()):
            outs = Parallel(-2)(delayed(test_chaotic)(independent, gamma, trial, N) for trial in range(300))
            with open('../results/sdcit_chaotic.csv', 'a') as f:
                for out in outs:
                    print(*out, sep=',', file=f, flush=True)

        for noise, independent, N in tqdm(postnonlinear_noise_configs()):
            outs = Parallel(-2)(delayed(test_postnonlinear)(independent, noise, trial, N) for trial in range(300))
            with open('../results/sdcit_postnonlinear.csv', 'a') as f:
                for out in outs:
                    print(*out, sep=',', file=f, flush=True)