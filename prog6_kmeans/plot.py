import os

import numpy as np
import matplotlib.pyplot as plt
from sklearn.decomposition import PCA

START_LOGFILE = "./start.log"
END_LOGFILE = "./end.log"
START_PLOTFILE = "./start.png"
END_PLOTFILE = "./end.png"

for title, logfile, plotfile in [("开始", START_LOGFILE, START_PLOTFILE), ("结束", END_LOGFILE, END_PLOTFILE)]:

    assert os.path.exists(logfile), "日志文件不存在，请先运行程序生成日志。"

    with open(logfile) as f:
        # 读取文件头
        M, N, K = f.readline().split(',')
        M, N, K = int(M), int(N), int(K)

        # 读取数据
        data = []
        cluster_assignments = []
        cluster_centroids = []

        for line in f.readlines():
            prefix = line.split(':')[0].split(' ')[0]
            if prefix == "Example":
                cluster_assignments.append(line.split(':')[0].split(' ')[-1])
                datapoint = line.split(':')[1].strip().split(' ')
                data.append(np.asarray(datapoint, dtype=float))
                line = f.readline()
            elif prefix == "Centroid":
                centroid = line.split(':')[1].strip().split(' ')
                cluster_centroids.append(np.asarray(centroid, dtype=float))

    # 主要的数据容器
    data = np.stack(data)
    cluster_assignments = np.asarray(cluster_assignments, dtype=int)
    cluster_centroids = np.stack(cluster_centroids)

    # 降维，以便绘制各 cluster
    pca = PCA(n_components=2)
    pca.fit(data)

    data_2d = pca.transform(data)
    cluster_centroids_2d = pca.transform(cluster_centroids)

    plt.subplots(figsize=(10,8))
    plt.scatter(data_2d[:,0], data_2d[:,1], c=cluster_assignments)
    plt.scatter(cluster_centroids_2d[:,0], cluster_centroids_2d[:,1], c='r', marker='*', edgecolors='black', s=1000)
    plt.title(f"K-Means: {title}")

    plt.savefig(plotfile)