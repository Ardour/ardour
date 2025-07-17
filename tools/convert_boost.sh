#!/bin/bash

for file in `ag -l 'boost::unordered_map' libs gtk2_ardour` ; do
	sed -i'' 's/boost::unordered_map/std::unordered_map/g' $file
	sed -i'' 's/#include <boost\/unordered_map.hpp>/#include <unordered_map>/' $file
done

for file in `ag -l 'BOOST_STATIC_ASSERT' libs gtk2_ardour` ; do
	sed -i'' 's/BOOST_STATIC_ASSERT/static_assert/g' $file
	sed -i'' '/#include <boost\/static_assert.hpp>/d' $file
done

for file in `ag -l 'BOOST_FOREACH' libs gtk2_ardour` ; do
	sed -i'' 's/BOOST_FOREACH *(\([^,]*\),\(.*\)/for (\1 :\2/' $file
	sed -i'' '/#include <boost\/foreach.hpp>/d' $file
done

for file in `ag -l 'boost::tuple' libs gtk2_ardour` ; do
	sed -i'' 's/boost::tuple/std::tuple/g' $file
	sed -i'' 's/#include <boost\/tuple\/tuple.hpp>/#include <tuple>/' $file
	sed -i'' '/#include <boost\/tuple\/tuple_comparison.hpp>/d' $file
done

for file in `ag -l 'boost::math::isnormal' libs gtk2_ardour` ; do
	sed -i'' 's/boost::math::isnormal/std::isnormal/' $file
	sed -i'' '/#include <boost\/math\/special_functions\/fpclassify.hpp>/d' $file
done

for file in `ag -l 'boost::container::set' libs gtk2_ardour` ; do
	sed -i'' 's/boost::container::set/std::set/' $file
	sed -i'' '/#include <boost\/container\/set.hpp>/d' $file
done

for file in `ag -l 'boost::none' libs gtk2_ardour` ; do
	sed -i'' 's/boost::none/std::nullopt/' $file
	sed -i'' '/#include <boost\/none.hpp>/d' $file
done

for file in `ag -l 'boost::optional' libs gtk2_ardour` ; do
	sed -i'' 's/boost::optional/std::optional/' $file
	sed -i'' 's/#include <boost\/optional.hpp>/#include <optional>/' $file
done

for file in `ag -l 'boost::function' libs gtk2_ardour luasession session_utils` ; do
	sed -i'' 's/boost::function1<\([^,]*\),\([^>]*\)>/std::function<\1 (\2)>/g' $file
	sed -i'' 's/boost::function0<\([^>]*\)>/std::function<\1 ()>/g' $file
	sed -i'' 's/boost::function0/std::function/g' $file
	sed -i'' 's/boost::function/std::function/g' $file
	sed -i'' '/#include <boost\/function.hpp>/d' $file
done

for file in `ag -l 'boost::bind' libs gtk2_ardour headless luasession` ; do
	sed -i'' 's/boost::bind/std::bind/g;s/boost::type<void> (),//' $file
done

for file in `ag -l 'boost::ref' libs gtk2_ardour` ; do
	sed -i'' 's/boost::ref/std::ref/g' $file
done

for file in `ag -l ' boost::lambda::' libs gtk2_ardour` ; do
	sed -i'' 's/ boost::lambda:://g' $file
done

for file in `ag -l 'boost/bind.hpp' libs gtk2_ardour` ; do
	sed -i'' '/#include <boost\/bind.hpp>/d' $file
done

for file in `ag -l 'boost/lambda/lambda.hpp' libs gtk2_ardour` ; do
	sed -i'' '/#include <boost\/lambda\/lambda.hpp>/d' $file
done

for file in `ag -l 'boost::scoped_array' libs gtk2_ardour` ; do
	sed -i'' 's/boost::scoped_array *<\([^>]*\)>/std::unique_ptr<\1[]>' $file
done
