#pragma once

#include <string>

/**
 * ToolBox - Utility class for reading configuration values from KUBE_config.mqh
 * 
 * This class provides static methods to read various configuration parameters
 * from the MQL5 config file using regex pattern matching.
 */
class ToolBox {
public:
    /**
     * Read an integer value from the config file.
     * 
     * @param path Path to the config file (e.g., KUBE_config.mqh)
     * @param param_name Name of the parameter to read (e.g., "c_fitness_lookback_EAs")
     * @return The integer value found in the config file
     * @throws Exits program if parameter not found or file cannot be opened
     */
    static int read_int_value(const std::string& path, const std::string& param_name);

    /**
     * Read a double value from the config file.
     * 
     * @param path Path to the config file (e.g., KUBE_config.mqh)
     * @param param_name Name of the parameter to read (e.g., "c_lambda", "c_alpha")
     * @return The double value found in the config file
     * @throws Exits program if parameter not found or file cannot be opened
     */
    static double read_double_value(const std::string& path, const std::string& param_name);

    /**
     * Read a string value from the config file.
     * 
     * @param path Path to the config file (e.g., KUBE_config.mqh)
     * @param param_name Name of the parameter to read
     * @return The string value found in the config file (without quotes)
     * @throws Exits program if parameter not found or file cannot be opened
     */
    static std::string read_string_value(const std::string& path, const std::string& param_name);

    /**
     * Get the default config path.
     * 
     * @return Default path to KUBE_config.mqh
     */
    static std::string default_config_path();

private:
    // Helper method to handle errors
    static void die(const std::string& msg, int rc = 1);
};
