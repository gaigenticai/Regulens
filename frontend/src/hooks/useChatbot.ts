import { useQuery, useMutation } from '@tanstack/react-query';
import { apiClient } from '@/services/api';
import type * as API from '@/types/api';

export function useChatbotConversations() {
  return useQuery({
    queryKey: ['chatbot-conversations'],
    queryFn: () => apiClient.getChatbotConversations(),
  });
}

export function useSendMessage() {
  return useMutation({
    mutationFn: (message: API.ChatbotMessage) => apiClient.sendChatbotMessage(message),
  });
}
