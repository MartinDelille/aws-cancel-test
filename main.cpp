#include <QFile>

#include <fstream>
#include <memory>

#include <aws/core/Aws.h>
#include <aws/core/utils/threading/Executor.h>
#include <aws/transfer/TransferManager.h>

int main(int argc,
         char *argv[]) {
	using namespace std;
	using namespace Aws;
	using namespace Transfer;

	if (argc < 5) {
		cout << "Not enough argument" << endl;
		return -1;
	}

	SDKOptions options;
	InitAPI(options);
	String bucket = argv[1];
	String key = argv[2];
	uint64_t contentLength = atoi(argv[3]);
	String fileName = argv[4];
	cout << bucket << " / " << key << " => " << fileName << endl;
	condition_variable condition;
	mutex m;
	shared_ptr<Utils::Threading::Executor>  executor = make_shared<Utils::Threading::DefaultExecutor>();
	TransferManagerConfiguration transferManagerConfiguration(executor.get());
	transferManagerConfiguration.transferStatusUpdatedCallback = [](const Transfer::TransferManager*,
	                                                                const shared_ptr<const Transfer::TransferHandle> &handle) {
		cout << handle->GetStatus() << endl;
	};

	transferManagerConfiguration.errorCallback = [](const Transfer::TransferManager*,
	                                                const shared_ptr<const Transfer::TransferHandle>&,
	                                                const Client::AWSError<S3::S3Errors> &error) {
		cout << error.GetMessage() << endl;
	};
	Client::ClientConfiguration s3ClientConfiguration;
	s3ClientConfiguration.region = Region::EU_WEST_1;

	transferManagerConfiguration.s3Client = make_shared<S3::S3Client>(s3ClientConfiguration);
	shared_ptr<TransferManager> transferManager = TransferManager::Create(transferManagerConfiguration);
	uint64_t offset = 0;
	FStream *fs = nullptr;
	auto createStreamFn = [fileName, offset, &fs, &condition, &m] {
		unique_lock<mutex> locker(m);
		cout << "creating " << fileName << endl;
#ifdef _MSC_VER
		fs = New<FStream>("aws-cancel-test", Utils::StringUtils::ToWString(fileName), ios_base::out);
#else
		fs = New<FStream>("aws-cancel-test", fileName.c_str(), ios_base::out);
#endif
		fs->seekp(offset);
		condition.notify_all();
		return fs;
	};
	unique_lock<mutex> locker(m);
	auto handle = transferManager->DownloadFile(bucket, key, offset, contentLength, createStreamFn);
	// Wait until the file is created
	condition.wait(locker);
	cout << "file created: " << handle->GetStatus() << " " << QFile(fileName.c_str()).size() * 100 / contentLength << " %" << endl;
	handle->Cancel();

	cout << "file canceled: " << handle->GetStatus() << " " << QFile(fileName.c_str()).size() * 100 / contentLength << " %" << endl;
	handle->WaitUntilFinished();
	cout << "file canceled: " << handle->GetStatus() << " " << QFile(fileName.c_str()).size() * 100 / contentLength << " %" << endl;

	ShutdownAPI(options);
	return 0;
}
