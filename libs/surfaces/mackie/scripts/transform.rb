class ElementHandler

	def apply( anElement )
		anElement.each {|e| handle(e)} if anElement
	end

	def handle( aNode )
		if aNode.kind_of? REXML::Text
			handleTextNode(aNode) 
		elsif aNode.kind_of? REXML::Element
			handle_element aNode  
		else
			return #ignore comments and processing instructions
		end
	end
  
	def handle_element( anElement )
		handler_method = "handle_" + anElement.name.tr("-","_")
		if self.respond_to? handler_method
			self.send(handler_method, anElement)
		else
			default_handler(anElement)  
		end
	end

end
