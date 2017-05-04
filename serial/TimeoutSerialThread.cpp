/*****************************************************************************/
/**
* \file	TimeoutSerialThread.cpp
*
* \author	Per Johansson
*
* Copyright &copy; Maquet Critical Care AB, Sweden
*
******************************************************************************/

#include "TimeoutSerialThread.hpp"
#include <string>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <boost/bind.hpp>
#include <boost/exception/exception.hpp>
#include <boost/exception/diagnostic_information.hpp>


TimeoutSerialThread::TimeoutSerialThread(const std::string& devname, std::uint32_t baudrate,
	boost::asio::serial_port_base::parity opt_parity,
	boost::asio::serial_port_base::character_size opt_csize,
	boost::asio::serial_port_base::flow_control opt_flow,
	boost::asio::serial_port_base::stop_bits opt_stop) :
	io_(),
	port_(io_),
	devname_(devname),
	baudrate_(baudrate),
	opt_parity_(opt_parity),
	opt_csize_(opt_csize),
	opt_flow_(opt_flow),
	opt_stop_(opt_stop),
	timer_(io_),
	timeout_(boost::posix_time::seconds(0)),
	result_(resultInProgress),
	bytesTransferred_(0),
	delim_(""),
	queue_(nullptr),
	isAlive_(true),
	stopRequested_(false)
{
}


TimeoutSerialThread::TimeoutSerialThread(const char *delim, ThreadSafeQueue<std::string *> *queue, const std::string& devname, std::uint32_t baudrate,
	boost::asio::serial_port_base::parity opt_parity,
	boost::asio::serial_port_base::character_size opt_csize,
	boost::asio::serial_port_base::flow_control opt_flow,
	boost::asio::serial_port_base::stop_bits opt_stop) :
	io_(),
	port_(io_),
	devname_(devname),
	baudrate_(baudrate),
	opt_parity_(opt_parity),
	opt_csize_(opt_csize),
	opt_flow_(opt_flow),
	opt_stop_(opt_stop),
	timer_(io_),
	timeout_(boost::posix_time::seconds(1)),
	result_(resultInProgress),
	bytesTransferred_(0),
	delim_(delim),
	queue_(queue),
	isAlive_(true),
	stopRequested_(false)
{
}

TimeoutSerialThread::~TimeoutSerialThread()
{
	close();
}

bool TimeoutSerialThread::open()
{
	if (isOpen())
	{
		close();
	}

	// workaround since open() needs extra time, or to be called twice when
	// the com port is opened a seccond time (after a communication timeout)
	bool success = false;
	int retries = 3;
	do
	{
		try
		{
			port_.open(devname_);
			success = true;
		}
		catch (boost::system::system_error&)
		{
			--retries;
		}
	} while (!success && retries > 0);

	if (success)
	{
		port_.set_option(boost::asio::serial_port_base::baud_rate(baudrate_));
		port_.set_option(opt_parity_);
		port_.set_option(opt_csize_);
		port_.set_option(opt_flow_);
		port_.set_option(opt_stop_);
		return true;
	}
	else
	{
		return false;
	}
}

bool TimeoutSerialThread::isOpen() const
{
	return port_.is_open();
}

void TimeoutSerialThread::close()
{
	if (isOpen())
	{
		port_.close();
	}
}

void TimeoutSerialThread::setTimeout(const boost::posix_time::time_duration& t)
{
	timeout_ = t;
}

void TimeoutSerialThread::write(const char *data, size_t size)
{
	boost::asio::write(port_, boost::asio::buffer(data, size));
}

void TimeoutSerialThread::write(const std::vector<char>& data)
{
	boost::asio::write(port_, boost::asio::buffer(&data[0], data.size()));
}

void TimeoutSerialThread::writeString(const std::string& s)
{
	boost::asio::write(port_, boost::asio::buffer(s.c_str(), s.size()));
}

void TimeoutSerialThread::operator()()
{
	//For this code to work, there should always be a timeout, so the
	//request for no timeout is translated into a very long timeout
	if (timeout_ != boost::posix_time::seconds(0))
	{
		timer_.expires_from_now(timeout_);
	}
	else
	{
		timer_.expires_from_now(boost::posix_time::hours(100000));
	}

	timer_.async_wait(boost::bind(&TimeoutSerialThread::timeoutExpired, this, boost::asio::placeholders::error));	// initiate timer

	// initiate lastMessageTime with current time...just to get started
	std::int64_t lastMessageTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

	result_ = resultInProgress;	// initial state
	bytesTransferred_ = 0;
	asyncReadUntil(delim_);	// wait for next message

	for (;;)
	{
		io_.run_one();
		switch (result_)
		{
		case resultSuccess:
		{
			port_.cancel();

			bytesTransferred_ -= delim_.size();	//Don't count delim
			std::istream is(&readData_);
			std::string *resultStr = new std::string(bytesTransferred_, '\0');	//Alloc string
			is.read(&(*resultStr)[0], bytesTransferred_);	//Fill values
			is.ignore(delim_.size());	//Remove delimiter from stream
			queue_->push(resultStr);

			// reset lastMessage timer
			lastMessageTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

			result_ = resultInProgress;
			bytesTransferred_ = 0;
			asyncReadUntil(delim_);	// wait for next message
			break;
		}

		case resultTimeoutExpired:
			timer_.cancel();

			if (isStopRequested() ||
				std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count() - lastMessageTime >= MESSAGE_TIMEOUT)
			{
				cleanup();		// ready to die...
				return;			// ...terminate thread
			}

			result_ = resultInProgress;	// continue reading/waiting for data
			timer_.expires_from_now(timeout_);	// restart timer
			timer_.async_wait(boost::bind(&TimeoutSerialThread::timeoutExpired, this, boost::asio::placeholders::error));
			break;

		case resultError:
			timer_.cancel();
			cleanup();		// ready to die...
			return;					// ...terminate thread
			break;

		case resultInProgress:
			break; //if resultInProgress remain in the loop

		default:
			timer_.cancel();
			cleanup();		// undefined state -> die
			return;			// ...terminate thread
			break;
		}
	}
}

void TimeoutSerialThread::requestStop()
{
	std::lock_guard<std::mutex> lock{ mutex_ };
	stopRequested_ = true;
}

bool TimeoutSerialThread::isAlive()
{
	std::lock_guard<std::mutex> lock{ mutex_ };
	return isAlive_;
}

bool TimeoutSerialThread::isStopRequested()
{
	std::lock_guard<std::mutex> lock{ mutex_ };
	return stopRequested_;
}

void TimeoutSerialThread::setAlive(const bool isAlive)
{
	std::lock_guard<std::mutex> lock{ mutex_ };
	isAlive_ = isAlive;
}


void TimeoutSerialThread::asyncReadUntil(const std::string& delim)
{
	boost::asio::async_read_until(port_, readData_, delim, boost::bind(
		&TimeoutSerialThread::readCompleted, this, boost::asio::placeholders::error,
		boost::asio::placeholders::bytes_transferred));
}

void TimeoutSerialThread::timeoutExpired(const boost::system::error_code& error)
{
	if (!error)
	{
		result_ = resultTimeoutExpired;
	}
	else
	{
		std::cout << "timeoutExpired: resultError" << std::endl;
		result_ = resultError;
	}
}

void TimeoutSerialThread::readCompleted(const boost::system::error_code& error,
	const size_t bytesTransferred)
{
	if (!error)
	{
		result_ = resultSuccess;
		this->bytesTransferred_ = bytesTransferred;
	}
	else
	{
		result_ = resultError;
	}
}

void TimeoutSerialThread::cleanup()
{
	io_.stop();
	port_.cancel();
	close();
	setAlive(false);	// ready to die...
}
