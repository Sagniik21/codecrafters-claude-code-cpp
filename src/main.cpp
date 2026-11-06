#include<cstdio>
#include <cstdlib>
#include<fstream>
#include <iostream>
#include<sstream>
#include <string>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

int main(int argc, char* argv[]) {
    if (argc < 3 || std::string(argv[1]) != "-p") {
        std::cerr << "Expected first argument to be '-p'" << std::endl;
        return 1;
    }

    std::string prompt = argv[2];

    if (prompt.empty()) {
        std::cerr << "Prompt must not be empty" << std::endl;
        return 1;
    }

    const char* api_key_env = std::getenv("OPENROUTER_API_KEY");
    const char* base_url_env = std::getenv("OPENROUTER_BASE_URL");

    std::string api_key = api_key_env ? api_key_env : "";
    std::string base_url = base_url_env ? base_url_env : "https://openrouter.ai/api/v1";

    if (api_key.empty()) {
        std::cerr << "OPENROUTER_API_KEY is not set" << std::endl;
        return 1;
    }

    json request_body = {
        {"model", "anthropic/claude-haiku-4.5"},
        {"messages", json::array({
            {{"role", "user"}, {"content", prompt}}
            // Step 2 starts here
             })},
        {"tools", json::array({
            {
                {"type", "function"},
                {"function", {
                    {"name", "Read"},
                    {"description", "Read and return the contents of a file"},
                    {"parameters", {
                        {"type", "object"},
                        {"properties", {
                            {"file_path", {
                                {"type", "string"},
                                {"description", "The path to the file to read"}
                            }}
                        }},
                        {"required", json::array({"file_path"})}
                    }}
                }}
            },
            {
                //Advertising write tool
                {"type", "function"},
        {"function", {
            {"name", "Write"},
            {"description", "Write content to a file"},
            {"parameters", {
                {"type", "object"},
                {"required", json::array({"file_path", "content"})},
                {"properties", {
                    {"file_path", {
                        {"type", "string"},
                        {"description", "The path of the file to write to"}
                    }},
                    {"content", {
                        {"type", "string"},
                        {"description", "The content to write to the file"}
                    }}
                }}
            }}
        }}
            },

            //Advertising the bash tool
            {
                {"type", "function"},
                {"function", {
                    {"name", "Bash"},
                    {"description", "Execute a bash command and return the output"},
                    {"parameters", {
                        {"type", "object"},
                        {"required", json::array({"command"})},
                        {"properties", {
                            {"command", {
                                {"type", "string"},
                                {"description", "The bash command to execute"}
                            }}
                        }}
                    }}
                }}
            }
            
            
        })}
    };
   while (true) {
        cpr::Response response = cpr::Post(
        cpr::Url{base_url + "/chat/completions"},
        cpr::Header{
            {"Authorization", "Bearer " + api_key},
            {"Content-Type", "application/json"}
        },
        cpr::Body{request_body.dump()}
    
    );


    if (response.status_code != 200) {
        std::cerr << "HTTP error: " << response.status_code << std::endl;
        return 1;
    }

    json result = json::parse(response.text);

    if (!result.contains("choices") || result["choices"].empty()) {
        std::cerr << "No choices in response" << std::endl;
        return 1;
    }

    // You can use print statements as follows for debugging, they'll be visible when running tests.
   // std::cerr << "Logs from your program will appear here!" << std::endl;

    // TODO: Uncomment the line below to pass the first stage
   //  std::cout << result["choices"][0]["message"]["content"].get<std::string>();

  //  return 0;
  json msg = result["choices"][0]["message"];

   /* if (msg.contains("tool_calls") && !msg["tool_calls"].empty()) {
        json tc = msg["tool_calls"][0];
        if (tc["function"]["name"].get<std::string>() == "Read") {
            json args = json::parse(tc["function"]["arguments"].get<std::string>());
            std::ifstream file(args["file_path"].get<std::string>());
            std::stringstream buf;
            buf << file.rdbuf();
            std::cout << buf.str();
        }
    } else if (msg.contains("content") && !msg["content"].is_null()) {
        std::cout << msg["content"].get<std::string>();
    }

    return 0;*/

    request_body["messages"].push_back(msg);

        if (msg.contains("tool_calls") && !msg["tool_calls"].empty()) {
            for (const auto& tc : msg["tool_calls"]) {
                if (tc["function"]["name"].get<std::string>() == "Read") {
                    json args = json::parse(tc["function"]["arguments"].get<std::string>());
                    std::ifstream file(args["file_path"].get<std::string>());
                    std::stringstream buf;
                    buf << file.rdbuf();

                    request_body["messages"].push_back({
                        {"role", "tool"},
                        {"tool_call_id", tc["id"].get<std::string>()},
                        {"content", buf.str()}
                    });
                
            }
        
        else if (tc["function"]["name"].get<std::string>() == "Write") {
                    json args = json::parse(tc["function"]["arguments"].get<std::string>());
                    std::ofstream file(args["file_path"].get<std::string>());
                    file << args["content"].get<std::string>();
                    file.close();

                    request_body["messages"].push_back({
                        {"role", "tool"},
                        {"tool_call_id", tc["id"].get<std::string>()},
                        {"content", "Successfully wrote to file"}
                    });
                }
            }
            //Integrating advertisement for bash tool
            else if (tc["function"]["name"].get<std::string>() == "Bash") {
                json args = json::parse(tc["function"]["arguments"].get<std::string>());
                std::string command = args["command"].get<std::string>();
                std::string output;

                // Execute the bash command and capture the output
                FILE* pipe = popen(command.c_str(), "r");
                if (!pipe) {
                    output = "Failed to execute command";
                } else {
                    char buffer[128];
                    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                        output += buffer;
                    }
                    pclose(pipe);
                }

                request_body["messages"].push_back({
                    {"role", "tool"},
                    {"tool_call_id", tc["id"].get<std::string>()},
                    {"content", output}
                });
       
       
             }     else {
            if (msg.contains("content") && !msg["content"].is_null()) {
                std::cout << msg["content"].get<std::string>();
            }
            break;
        }
    }
    return 0;
}

