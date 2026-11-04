
import re
from scipy.linalg import eigh
from sklearn.cluster import KMeans, SpectralClustering
from sklearn.metrics import silhouette_score
import numpy as np 

"""
    Genome Fragment Clustering Analysis:

    Read genome fragment data

    Compute Laplacian matrix

    Spectral clustering

"""


class Frags:
    def __init__(self):
        self.n = 0
        self.frags_infos = np.zeros((self.n, 4))
        self.adj_matrix = np.zeros((self.n, self.n, 5)) 
    
    def ini_size(self, n:int):
        self.n = n
        self.frags_infos = np.zeros((self.n, 4))
        self.adj_matrix = np.zeros((self.n, self.n, 5)) # adjacency matrix
    
    def read_frags_adj_matrix(self, frags_file):
        """
            Read genome fragment data.
            :param frags_file: genome fragment file
            :return: None
        """
        with open(frags_file, 'r') as f:
            while (line := f.readline()):
                if re.match(r'# frags num:', line):
                    n = int(line.split(':')[1].strip())
                    self.ini_size(n)
                elif re.match(r'# frags_index', line):
                    for i in range(self.n):
                        line_tmp = f.readline().strip().split()
                        self.frags_infos[i] = [int(j) for j in line_tmp]
                elif re.match(r'# Matrix shape:', line):
                    shape = [int(j) for j in line.split(':')[1].strip().split()]
                    if shape[0] != self.n or shape[1] != self.n or shape[2] != 5:
                        raise ValueError("Matrix shape is not correct")
                    for layer_tmp in range(5):
                        line = f.readline().strip()
                        if not re.match(r'# layer:', line): 
                            raise ValueError(f"Matrix shape is not correct, line: {line}")
                        for row_tmp in range(self.n):
                            line_tmp = f.readline().strip().split()
                            self.adj_matrix[row_tmp, :, layer_tmp] = [float(j) for j in line_tmp]
                elif line == '\n': continue
                else:
                    print(f"[Frags file reading :: Warning]: {line}")

        print("[Frags file reading :: Status]: Read file successfully")  
    
    
    def kmeans_cluster(self, k, matrix:np.ndarray = None):
        """
            Compute eigenvalues/eigenvectors and run spectral clustering.
            :return: None
        """
        if matrix is None: 
            raise ValueError("Matrix is None, please provide a matrix")
        processed_adj_matrix = np.max(matrix[..., :4], axis=-1)
        print(f"[KMeans]: original matrix shape = {matrix.shape} processed matrix shape = {processed_adj_matrix.shape}")
        # Spectral clustering
        model = SpectralClustering(
            n_clusters=k,
            affinity='precomputed',  # similarity mode (Gaussian kernel or k-NN)
            assign_labels='kmeans'         # cluster in the low-dimensional space with K-Means
        )
        clusters = model.fit_predict(processed_adj_matrix)
        return  clusters
    
    def auto_kmeans_cluster(self, k_range=range(2, 20), matrix: np.ndarray = None):
        """
            Choose the best cluster count automatically and cluster (silhouette score).
        """
        if matrix is None:
            matrix = self.adj_matrix
        processed_adj_matrix = np.max(matrix[..., :4], axis=-1)
        degrees = np.sum(processed_adj_matrix, axis=1)
        D = np.diag(degrees)
        lap_matrix = D - processed_adj_matrix

        eigenvalues, eigenvectors = eigh(lap_matrix)
        
        best_k = None
        best_score = -1
        best_labels = None
        
        for k in k_range:
            if k >= matrix.shape[0]: break
            kmeans = KMeans(n_clusters=k, n_init=10, random_state=0)
            labels = kmeans.fit_predict(eigenvectors[:, :k])
            score = silhouette_score(eigenvectors[:, :k], labels)
            if score > best_score:
                best_k = k
                best_score = score
                best_labels = labels

        print(f"[Auto-KMeans]: Best K = {best_k}, silhouette score = {best_score:.4f}")
        return best_labels
    


def run_kmeans(k = 6, matrix:np.ndarray = None):
    """
        Run k-means / spectral clustering.
        :return: None
    """
    frags = Frags()
    if matrix is None:
        raise ValueError("Matrix is None, please provide a matrix")
        frags.read_frags_adj_matrix("/Users/sg35/PretextView/build_cmake/PretextViewAI.app/Contents/Resources/frags_info_likelihood_table.txt")
        
        return frags.kmeans_cluster(k).astype(np.int32)
    else:
        return frags.kmeans_cluster(k, matrix).astype(np.int32)




    
if __name__ == "__main__":
    frags = Frags()
    frags.read_frags_adj_matrix("/Users/sg35/PretextView/build_cmake/PretextViewAI.app/Contents/Resources/frags_info_likelihood_table.txt")
    lables = frags.kmeans_cluster(10)
    print(f"labels = {lables}")

