mkdir -p `pkg-config --variable=gtk_binary_version gtk+-2.0`/`pkg-config --variable=gtk_host gtk+-2.0`/engines
cp libclearlooks.so `pkg-config --variable=gtk_binary_version gtk+-2.0`/`pkg-config --variable=gtk_host gtk+-2.0`/engines
