#include <boost/asio.hpp>
#include <config/ServerConfig.hpp>
#include <redis/RedisManager.hpp>
#include <grpc/GrpcBalancerImpl.hpp>
#include <thread>

int main() {
          [[maybe_unused]] auto& redis = redis::RedisConnectionPool::get_instance();

  std::string address =
      fmt::format("{}:{}", ServerConfig::get_instance()->BalanceServiceAddress,
                  ServerConfig::get_instance()->BalanceServicePort);

  spdlog::info("Balance RPC Server Started Running On {}", address);

  /*gRPC server*/
  grpc::ServerBuilder builder;
  grpc::GrpcBalancerImpl impl;

  /*binding ports and establish service*/
  builder.AddListeningPort(address, grpc::InsecureServerCredentials());
  builder.RegisterService(&impl);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

  try {
    /*setting up signal*/
    boost::asio::io_context ioc;
    boost::asio::signal_set signal{ioc, SIGINT, SIGTERM};
    signal.async_wait(
        [&ioc, &server](boost::system::error_code ec, int sig_number) {
          if (ec) {
            return;
          }
          spdlog::critical("balance server exit due to control-c input!");
          ioc.stop();
          server->Shutdown();
        });

    // ioc should not be blocked by server->wait()
    std::thread([&ioc]() { ioc.run(); }).detach();

    server->Wait();
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
  }
  return 0;
}
