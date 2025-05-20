#pragma once
#include <memory>
#include <array>
#include <string>

namespace lepton_wrapper
{
	class library
	{
		HMODULE handle = {};

	public:
		library(const CString& path)
			: handle(::LoadLibrary(path))
		{
		}
		~library()
		{
			if (handle)
			{
				::FreeLibrary(handle);
			}
		}
		operator HMODULE() const
		{
			return handle;
		}
	};

	class lib_lepton
	{
		library lib;

		// Helper function to get a procudere by its name.
		template <typename FN>
		FN function(LPCSTR name) const
		{
			if (FN fn = (FN)::GetProcAddress(lib, name); fn != nullptr)
			{
				return fn;
			}
			return nullptr;
		}

		// Call get() to optain an instance of the lib_lepton class.
		lib_lepton(const CString& path)
			: lib(path)
		{}

		// It is non-copyable.
		lib_lepton(const lib_lepton& rhs);

	public:
		static const lib_lepton& get();

		/// C ABI interface for compressing image, exposed from DLL
		int32_t WrapperCompressImage(const uint8_t* input_buffer,
			uint64_t input_buffer_size,
			uint8_t* output_buffer,
			uint64_t output_buffer_size,
			int32_t number_of_threads,
			uint64_t* result_size) const
		{
			typedef int32_t(__cdecl* fn_WrapperCompressImage)(const uint8_t*, uint64_t, uint8_t*, uint64_t, int32_t, uint64_t*);
			if (auto fn = function<fn_WrapperCompressImage>("WrapperCompressImage"))
			{
				return fn(input_buffer, input_buffer_size, output_buffer, output_buffer_size, number_of_threads, result_size);
			}
			return 0;
		}

		/// C ABI interface for decompressing image, exposed from DLL
		/// 
		/// * number_of_threads - 0 for automatically selected by the library, or specify it manually.
		int32_t WrapperDecompressImage(const uint8_t* input_buffer,
			uint64_t input_buffer_size,
			uint8_t* output_buffer,
			uint64_t output_buffer_size,
			int32_t number_of_threads,
			uint64_t* result_size) const
		{
			typedef int32_t(__cdecl* fn_WrapperDecompressImage)(const uint8_t*, uint64_t, uint8_t*, uint64_t, int32_t, uint64_t*);
			if (auto fn = function<fn_WrapperDecompressImage>("WrapperDecompressImage"))
			{
				return fn(input_buffer, input_buffer_size, output_buffer, output_buffer_size, number_of_threads, result_size);
			}
			return 0;
		}

		/// C ABI interface for decompressing image, exposed from DLL.
		/// use_16bit_dc_estimate argument should be set to true only for images
		/// that were compressed by C++ version of Leptron (see comments below).
		/// 
		/// * number_of_threads - 0 for automatically selected by the library, or specify it manually.
		int32_t WrapperDecompressImageEx(const uint8_t* input_buffer,
			uint64_t input_buffer_size,
			uint8_t* output_buffer,
			uint64_t output_buffer_size,
			int32_t number_of_threads,
			uint64_t* result_size,
			bool use_16bit_dc_estimate) const
		{
			typedef int32_t(__cdecl* fn_WrapperDecompressImageEx)(const uint8_t*, uint64_t, uint8_t*, uint64_t, int32_t, uint64_t*, bool);
			if (auto fn = function<fn_WrapperDecompressImageEx>("WrapperDecompressImageEx"))
			{
				return fn(input_buffer, input_buffer_size, output_buffer, output_buffer_size, number_of_threads, result_size, use_16bit_dc_estimate);
			}
			return 0;
		}

		void get_version(std::string& package_, std::string& git_) const
		{
			typedef void(__cdecl* fn_get_version)(const char**, const char**);
			if (auto fn = function<fn_get_version>("get_version"))
			{
				const char* a1 = nullptr;
				const char* a2 = nullptr;
				fn(&a1, &a2);
				package_.assign(a1, 5);
				git_.assign(a2, 40);
			}
		}

		void* create_decompression_context(uint32_t features) const
		{
			typedef void* (__cdecl* fn_create_decompression_context)(uint32_t);
			if (auto fn = function<fn_create_decompression_context>("create_decompression_context"))
			{
				return fn(features);
			}
			return nullptr;
		}

		void free_decompression_context(void* context) const
		{
			typedef void(__cdecl* fn_free_decompression_context)(void*);
			if (auto fn = function<fn_free_decompression_context>("free_decompression_context"))
			{
				fn(context);
			}
		}

		/// partially decompresses an image from a Lepton file.
		///
		/// Returns -1 if more data is needed or if there is more data available, or 0 if done successfully.
		/// Returns > 0 if there is an error
		int32_t decompress_image(void* context,
			const uint8_t* input_buffer,
			uint64_t input_buffer_size,
			bool input_complete,
			uint8_t* output_buffer,
			uint64_t output_buffer_size,
			uint64_t* result_size,
			unsigned char* error_string,
			uint64_t error_string_buffer_len) const
		{
			typedef int32_t(__cdecl* fn_decompress_image)(void*, const uint8_t*, uint64_t, bool, uint8_t*, uint64_t, uint64_t*, unsigned char*, uint64_t);
			if (auto fn = function<fn_decompress_image>("decompress_image"))
			{
				return fn(context, input_buffer, input_buffer_size, input_complete, output_buffer, output_buffer_size, result_size, error_string, error_string_buffer_len);
			}
			return 0;
		}
	};

	bool LeptonSupport();
	// Gets the path where the global INI file and the EXE is located
	CString GetLibPath();
}
