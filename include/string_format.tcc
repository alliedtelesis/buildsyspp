namespace buildsys {
	/** @brief A C++ style replacement for C style sprintf/asprintf. Returns a formatted
	 *         string based on the input format and arguments.
	 *
	 * @param format The format to print with.
	 * @param args (variadic) The arguments to use with the format.
	 * @return The formatted string.
	 */
	template < typename ... Args >
	    std::string string_format(const std::string & format, Args ... args) {
		int size = snprintf(nullptr, 0, format.c_str(), args ...) + 1;	// Extra space for '\0'
		std::unique_ptr < char[] > buf(new char[size]);
		snprintf(buf.get(), static_cast<size_t>(size), format.c_str(), args ...);
		return std::string(buf.get(), buf.get() + size - 1);	// We don't want the '\0' inside
	}
}