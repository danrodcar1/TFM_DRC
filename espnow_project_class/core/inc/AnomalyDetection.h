#pragma once

#include <cmath>
#include <vector>
#include <bits/stdc++.h>

using namespace std;

class AnomalyDetection_t
{
    static AnomalyDetection_t *this_object;

    // Others control vars

    int a_data, n_dim, b_data;
    double distance;
    // User control vars
    vector<vector<double>> dtw_vector;

public:
    AnomalyDetection_t()
    {
        this_object = this;
    }

    // Function that compute mean of a vector of 2bytes
    double calculate_mean(const vector<u_int16_t> &data)
    {
        double sum = 0.0;
        for (const u_int16_t &value : data)
        {
            sum += value;
        }
        return sum / data.size();
    }

    // Function that compute Pearson Correlation
    double compute_pearson_correlation(const vector<u_int16_t> &x, const vector<u_int16_t> &y)
    {
        // Samples should be of the same size
        if (x.size() != y.size())
        {
            ESP_LOGI("* Pearson correlation", "Samples should be of the same size");
            return 0;
        }

        double meanX = calculate_mean(x);
        double meanY = calculate_mean(y);
        double sumProduct = 0.0;
        double sumXSquare = 0.0;
        double sumYSquare = 0.0;

        for (size_t i = 0; i < x.size(); ++i)
        {
            double deltaX = x[i] - meanX;
            double deltaY = y[i] - meanY;
            sumProduct += deltaX * deltaY;
            sumXSquare += deltaX * deltaX;
            sumYSquare += deltaY * deltaY;
        }
        double correlation = sumProduct / (sqrt(sumXSquare) * sqrt(sumYSquare));
        return correlation;
    }

    double get_fisherz_tran(double r)
    {
        // z = 1/2*((1+r)/(1-r)) = atanh(r);
        return 0.5 * log((1 + r) / (1 - r));
    }

    double get_inv_fisherz_tran(double z)
    {
        // r=(exp(2*z)-1)/(exp(2*z)+1) = tanh(z);
        return (exp(2 * z) - 1) / (exp(2 * z) + 1);
    }

    double typical_deviation(const vector<u_int16_t> &x)
    {
        double meanX = calculate_mean(x);
        double sumXSquare = 0.0;
        for (size_t i = 0; i < x.size(); i++)
        {
            double deltaX = x[i] - meanX;
            sumXSquare += deltaX * deltaX;
        }
        return sqrt(sumXSquare / x.size());
    }

    vector<double> pearson_based_LISA(vector<vector<uint16_t>> &X, uint8_t window_size, uint8_t time_series_index_p, double threshold)
    {

        // 	Input :X = {X1 , . . . , Xm }: a set of m univariate time series where Xp = [vp1 , . . . , vpn ] is a
        // series of n temporal ordered values vpi ,
        // w: the length of the moving window,
        // p ∈ {1, . . . , m}: the index of the time series to perform anomaly detection,
        // δ: a threshold that defines the decision boundary for outlier scores
        // For the exercice: X = time_series_set; w = window_size = 4; p = time_series_index_p = 2;

        // https://www.daniweb.com/programming/software-development/threads/230490/how-to-extract-a-row-of-a-2d-vector#post1015847
        // https://www.educative.io/answers/how-to-implement-a-sliding-window-algorithm-in-cpp

        ESP_LOGI(TAG, "set size:%d", X.size());
        ESP_LOGI(TAG, "time series size of our set size:%d", X[0].size());
        uint8_t itLvpi = window_size;
        uint8_t start = 0;
        vector<double> L_v_p_i(X[0].size() - window_size);
        vector<uint16_t> index_subset_row_p(window_size);
        for (size_t i = window_size; i < X[0].size(); i = i + 1)
        {
            vector<uint16_t> index_subset_col(X.size());
            double sum = 0.0;
            double mean_vi = 0.0;
            for (size_t j = 0; j < X.size(); j++)
            {
                for (size_t k = 0; k < X[0].size(); k++)
                {
                    if (k == i)
                    {
                        mean_vi += X[j][k];
                        index_subset_col[j] = X[j][k];
                        ESP_LOGI(TAG, "index_subset_col[%d]:%d", j, index_subset_col[j]);
                    }
                }
            }
            mean_vi /= X.size();
            // we have to point to the element [time_series_index_index_p - 1][window_size] since in C++ arrays start at 0
            uint8_t v_p_i = X[time_series_index_p - 1][i];
            double typ_dev = typical_deviation(index_subset_col);
            ESP_LOGI(TAG, "mean_vi:%f", mean_vi);
            ESP_LOGI(TAG, "v_p_i:%d", v_p_i);
            double z_p_i = (v_p_i - mean_vi) / typ_dev;
            ESP_LOGI(TAG, "z_p_i:%f", z_p_i);
            uint8_t itXp = window_size;
            for (size_t j = 0; j < window_size; j++)
            {
                index_subset_row_p[j] = X[time_series_index_p - 1][itXp + start - window_size + 1];
                ESP_LOGI(TAG, "index_subset_row_p[%d]:%d", j, index_subset_row_p[j]);
                itXp = itXp + 1;
            }

            for (size_t q = 0; q < X.size(); q++)
            {
                vector<uint16_t> index_subset_row_q(window_size);
                uint8_t itXq = window_size;
                if (q != time_series_index_p - 1)
                {
                    for (size_t j = 0; j < window_size; j++)
                    {
                        index_subset_row_q[j] = X[q][itXq + start - window_size + 1];
                        ESP_LOGI(TAG, "index_subset_row_q[%d]:%d", j, index_subset_row_q[j]);
                        itXq = itXq + 1;
                    }
                    uint8_t v_q_i = X[q][i];
                    ESP_LOGI(TAG, "v_q_i:%d", v_q_i);
                    double z_q_i = (v_q_i - mean_vi) / typ_dev;
                    ESP_LOGI(TAG, "z_q_i:%f", z_q_i);
                    double w_p_q_i = abs(compute_pearson_correlation(index_subset_row_p, index_subset_row_q));
                    ESP_LOGI(TAG, "w_p_q_i:%f", w_p_q_i);
                    sum = sum + w_p_q_i * z_q_i;
                }
            }
            start++;
            ESP_LOGI(TAG, "sum:%f", sum);

            L_v_p_i[itLvpi - window_size] = z_p_i * sum;
            // if (L_v_p_i[itLvpi - window_size] < threshold)
            // {
            //     L_v_p_i[itLvpi - window_size]
            // }
            ESP_LOGI(TAG, "L_v_p_i[%d]:%f", itLvpi - window_size, L_v_p_i[itLvpi - window_size]);
            itLvpi = itLvpi + 1;
        }
        return L_v_p_i;
    }

    vector<double> DTW_based_LISA(vector<vector<uint16_t>> &X, uint8_t window_size, uint8_t time_series_index_p, double threshold)
    {

        // 	Input :X = {X1 , . . . , Xm }: a set of m univariate time series where Xp = [vp1 , . . . , vpn ] is a
        // series of n temporal ordered values vpi ,
        // w: the length of the moving window,
        // p ∈ {1, . . . , m}: the index of the time series to perform anomaly detection,
        // δ: a threshold that defines the decision boundary for outlier scores
        // For the exercice: X = time_series_set; w = window_size = 4; p = time_series_index_p = 2;

        // https://www.daniweb.com/programming/software-development/threads/230490/how-to-extract-a-row-of-a-2d-vector#post1015847
        // https://www.educative.io/answers/how-to-implement-a-sliding-window-algorithm-in-cpp

        ESP_LOGI(TAG, "set size:%d", X.size());
        ESP_LOGI(TAG, "time series size of our set size:%d", X[0].size());
        uint8_t itLvpi = window_size;
        uint8_t start = 0;
        vector<double> L_v_p_i(X[0].size() - window_size);
        vector<uint16_t> index_subset_row_p(window_size);
        for (size_t i = window_size; i < X[0].size(); i = i + 1)
        {
            vector<uint16_t> index_subset_col(X.size());
            double sum = 0.0;
            double mean_vi = 0.0;
            for (size_t j = 0; j < X.size(); j++)
            {
                for (size_t k = 0; k < X[0].size(); k++)
                {
                    if (k == i)
                    {
                        mean_vi += X[j][k];
                        index_subset_col[j] = X[j][k];
                        ESP_LOGI(TAG, "index_subset_col[%d]:%d", j, index_subset_col[j]);
                    }
                }
            }
            mean_vi /= X.size();
            // we have to point to the element [time_series_index_index_p - 1][window_size] since in C++ arrays start at 0
            uint8_t v_p_i = X[time_series_index_p - 1][i];
            double typ_dev = typical_deviation(index_subset_col);
            ESP_LOGI(TAG, "mean_vi:%f", mean_vi);
            ESP_LOGI(TAG, "v_p_i:%d", v_p_i);
            double z_p_i = (v_p_i - mean_vi) / typ_dev;
            ESP_LOGI(TAG, "z_p_i:%f", z_p_i);
            uint8_t itXp = window_size;
            for (size_t j = 0; j < window_size; j++)
            {
                index_subset_row_p[j] = X[time_series_index_p - 1][itXp + start - window_size + 1];
                ESP_LOGI(TAG, "index_subset_row_p[%d]:%d", j, index_subset_row_p[j]);
                itXp = itXp + 1;
            }

            for (size_t q = 0; q < X.size(); q++)
            {
                vector<uint16_t> index_subset_row_q(window_size);
                uint8_t itXq = window_size;
                if (q != time_series_index_p - 1)
                {
                    for (size_t j = 0; j < window_size; j++)
                    {
                        index_subset_row_q[j] = X[q][itXq + start - window_size + 1];
                        ESP_LOGI(TAG, "index_subset_row_q[%d]:%d", j, index_subset_row_q[j]);
                        itXq = itXq + 1;
                    }
                    // DTW introduction for weighted coefficient
                    vector<vector<double>> cost;
                    vector<vector<int>> mypath;
                    cost = compute_accumulated_cost_matrix(index_subset_row_q, index_subset_row_p);
                    mypath = path();
                    // mypath = {{1, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 4}};
                    vector<uint16_t> index_subset_row_q_warped(mypath.size()), index_subset_row_p_warped(mypath.size());
                    uint8_t itXw = 1;
                    ESP_LOGI(TAG, "DTW Path:");
                    for (int w = 0; w < mypath.size(); ++w)
                    {
                        index_subset_row_p_warped[w] = index_subset_row_p[itXw - w + mypath[w][1] - 1]; // itXw - w + mypath[w][1] - 2
                        index_subset_row_q_warped[w] = index_subset_row_q[itXw - w + mypath[w][0] - 1];
                        ESP_LOGI(TAG, "i:%d j:%d", mypath[w][0], mypath[w][1]);
                        ESP_LOGI(TAG, "itXw:%d", itXw);
                        ESP_LOGI(TAG, "indexp:%d", itXw - w + mypath[w][1] - 1);
                        ESP_LOGI(TAG, "indexq:%d", itXw - w + mypath[w][0] - 1);
                        ESP_LOGI(TAG, "cost:%f", cost[mypath[w][0]][mypath[w][1]]);
                        ESP_LOGI(TAG, "index_subset_row_p_warped[%d]:%d", w, index_subset_row_p_warped[w]);
                        ESP_LOGI(TAG, "index_subset_row_q_warped[%d]:%d", w, index_subset_row_q_warped[w]);
                        itXw = itXw + 1;
                    }
                    // END of -DTW introduction for weighted coefficient
                    uint8_t v_q_i = X[q][i];
                    ESP_LOGI(TAG, "v_q_i:%d", v_q_i);
                    double z_q_i = (v_q_i - mean_vi) / typ_dev;
                    ESP_LOGI(TAG, "z_q_i:%f", z_q_i);
                    double w_p_q_i = abs(compute_pearson_correlation(index_subset_row_p_warped, index_subset_row_q_warped));
                    ESP_LOGI(TAG, "w_p_q_i:%f", w_p_q_i);
                    sum = sum + w_p_q_i * z_q_i;
                }
            }
            start++;
            ESP_LOGI(TAG, "sum:%f", sum);

            L_v_p_i[itLvpi - window_size] = z_p_i * sum;
            // if (L_v_p_i[itLvpi - window_size] < threshold)
            // {
            //     L_v_p_i[itLvpi - window_size]
            // }
            ESP_LOGI(TAG, "L_v_p_i[%d]:%f", itLvpi - window_size, L_v_p_i[itLvpi - window_size]);
            itLvpi = itLvpi + 1;
        }
        return L_v_p_i;
    }

    vector<vector<double>> compute_p_norm_distance_matrix(const vector<u_int16_t> &x, const vector<u_int16_t> &y, double p = 2.0)
    {
        // Calculate distance matrix
        //
        /*
         * Compute the p_norm between two 1D c++ vectors.
         *
         * The p_norm is sometimes referred to as the Minkowski norm. Common
         * p_norms include p=2.0 for the euclidean norm, or p=1.0 for the
         * manhattan distance. See also
         * https://en.wikipedia.org/wiki/Norm_(mathematics)#p-norm
         *
         * @a 1D vector of m size, where m is the number of dimensions.
         * @b 1D vector of m size (must be the same size as b).
         * @p value of norm to use.
         */
        // The sequences can have different lengths.

        // Allocate memory for the result
        vector<vector<double>> dist(y.size(), vector<double>(x.size(), 0));
        for (size_t i = 0; i < y.size(); i++)
        {
            for (size_t j = 0; j < x.size(); j++)
            {
                dist[i][j] = pow(pow((x[j] - y[i]), p), 1.0 / p);
            }
        }
        return dist;
    }

    vector<vector<double>> compute_accumulated_cost_matrix(const vector<u_int16_t> &x, const vector<u_int16_t> &y)
    {
        /*
         * Assembles a 2D c++ DTW distance vector.
         *
         * The DTW distance vector represents the matrix of DTW distances for
         * all possible alignments. The c++ vectors must have the same 2D size.
         * d.size() == c.size() == number of a data points, where d[0].size ==
         * c[0].size() == number of b data points.
         *
         * @d 2D DTW distance vector of [a data points][b data points].
         * @c 2D pairwise distance vector between every a and b data point.
         */
        a_data = x.size();
        b_data = y.size();
        // Compute accumulated cost matrix for warp path using Euclidean distance
        vector<vector<double>> distances(y.size(), vector<double>(x.size(), 0));
        distances = compute_p_norm_distance_matrix(x, y);
        for (size_t i = 0; i < y.size(); i++)
        {
            for (size_t j = 0; j < x.size(); j++)
            {
                ESP_LOGI(TAG, "distances[%d][%d]:%f", i, j, distances[i][j]);
            }
        }

        // Initialization
        // Allocate memory for the result
        vector<vector<double>> cost(y.size(), vector<double>(x.size(), 0));
        cost[0][0] = distances[0][0];
        for (size_t i = 1; i < y.size(); i++)
        {
            cost[i][0] = distances[i][0] + cost[i - 1][0];
        }
        for (size_t i = 1; i < x.size(); i++)
        {
            cost[0][i] = distances[0][i] + cost[0][i - 1];
        }
        // Accumulated warp path cost
        for (size_t i = 1; i < y.size(); i++)
        {
            for (size_t j = 1; j < x.size(); j++)
            {
                cost[i][j] = min({
                                 cost[i - 1][j],    // insertion
                                 cost[i][j - 1],    // deletion
                                 cost[i - 1][j - 1] // match
                             }) +
                             distances[i][j];
            }
        }
        for (size_t i = 0; i < y.size(); i++)
        {
            for (size_t j = 0; j < x.size(); j++)
            {
                ESP_LOGI(TAG, "cost[%d][%d]:%f", i, j, cost[i][j]);
            }
        }
        dtw_vector = cost;
        distance = cost[y.size() - 1][x.size() - 1];
        ESP_LOGI(TAG, "distance:%f", distance);
        return cost;
    }

    vector<vector<int>> path()
    {
        int i = a_data - 1;
        int j = b_data - 1;
        vector<vector<int>> my_path = {{i, j}};
        while (i > 0 || j > 0)
        {
            if (i == 0)
            {
                j -= 1;
            }
            else if (j == 0)
            {
                i -= 1;
            }
            else
            {
                double temp_step = min({dtw_vector[i - 1][j],
                                        dtw_vector[i][j - 1],
                                        dtw_vector[i - 1][j - 1]});
                if (temp_step == dtw_vector[i - 1][j])
                {
                    i -= 1;
                }
                else if (temp_step == dtw_vector[i][j - 1])
                {
                    j -= 1;
                }
                else
                {
                    i -= 1;
                    j -= 1;
                }
            }
            my_path.push_back({i, j});
        }
        reverse(my_path.begin(), my_path.end());
        return my_path;
    }
    // https://github.com/cjekel/DTW_cpp/blob/master/include/DTW.hpp
    // https://builtin.com/data-science/dynamic-time-warping
    // https://github.com/slaypni/fastdtw/tree/master?tab=readme-ov-file#id2
    // https://cs.fit.edu/~pkc/papers/tdm04.pdf
    // https://stackoverflow.com/questions/66768974/dynamic-time-warping-in-c
private:
};
AnomalyDetection_t *AnomalyDetection_t::this_object = NULL;
